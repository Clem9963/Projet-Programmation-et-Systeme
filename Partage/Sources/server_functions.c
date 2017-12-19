#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "server.h"
#include "server_functions.h"

int listenSocket(int port)
{
	int passive_sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sin; /* structure qui possède toutes les infos pour le socket */
	memset(&sin, 0, sizeof(sin));
	
	if (passive_sock == SOCKET_ERROR)
	{
		perror("socket error");
		exit(errno);
	}

	sin.sin_addr.s_addr = htonl(INADDR_ANY); /* on accepte toute adresse */
	sin.sin_port = htons(port); /* le port */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */

	if (bind(passive_sock, (struct sockaddr*) &sin, sizeof(sin)) == SOCKET_ERROR) /* on lie le socket à sin */
	{
		perror("bind error");
		exit(errno);
	}

	if (listen(passive_sock, 1) == SOCKET_ERROR) /* notre socket est prêt à écouter une connexion à la fois */
	{
    	perror("listen error");
    	exit(errno);
	}

	return passive_sock;
}

int getClient(struct Client *clients, int clients_nb, char *buffer)
{
	/* Fonction retournant l'indice du client dont l'username est celui de la requête /sendto */
	/* La fonction retourne -1 s'il n'u a aucun client correspondant */
	/* Elle prend en argument le tableau des clients, le nombre de clients actifs */
	/* et la requête /sendto dans buffer qui est une chaîne de caractères */

	char username[BUFFER_SIZE] = "";		// Il faudra penser à réadapter la fonction pour qu'elle soit plus optimisée
	int found = 0;
	int i = 0;

	strcpy(username, strchr(buffer, ' ') + 1);
	*(strchr(username, ' ')) = '\0';
	while (!found && i < clients_nb)
	{
		found = !strcmp(username, clients[i].username);
		i++;
	}

	if (found)
	{
		i--;
		return i;
	}
	else
	{
		return -1;
	}
}

void *transferControl(void *src_data)
{
	/* Fonction gérant le transit du fichier par le serveur */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";
	char *char_ptr = NULL;
	int answer = 0;
	int package_number = 0;
	int residue_size = 0;
	int i = 0;

	while(pthread_mutex_lock(data->mutex_thread_status) == EDEADLK)
	{
		continue;
	}

	sendClient(data->receiving_client.msg_client_sock, data->request, strlen(data->request)+1);
	recvClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));

	answer = atoi(buffer);
	if (!answer)
	{
		sprintf(buffer, "%d", answer);
		sendClient(data->sending_client.file_client_sock, buffer, strlen(buffer)+1);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		printf("Refus du transfert\n");
		pthread_exit(NULL);
	}

	printf("Le transfert a été accepté par le client destinataire\n");

	sendClient(data->sending_client.file_client_sock, buffer, strlen(buffer)+1);

	recvClient(data->sending_client.file_client_sock, buffer, sizeof(buffer));		// Réception du nombre de paquets et de la taille du dernier paquet
	sendClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));
	char_ptr = strchr(buffer, ' ');
	*char_ptr = '\0';
	package_number = atoi(buffer);
	residue_size = atoi(char_ptr + 1);
	recvClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));		// Réception de l'accusé de réception
	sendClient(data->sending_client.file_client_sock, buffer, 1);						// Accusé de réception pour la synchronisation

	for (i = 0; i < package_number; i++)
	{
		recvClient(data->sending_client.file_client_sock, buffer, sizeof(buffer));
		sendClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));

		/* Accusé de réception (pour la synchronisation) */
		recvClient(data->receiving_client.file_client_sock, buffer, 1);
		sendClient(data->sending_client.file_client_sock, buffer, 1);
	}
	if (residue_size != 0)
	{
		recvClient(data->sending_client.file_client_sock, buffer, sizeof(buffer));
		sendClient(data->receiving_client.file_client_sock, buffer, residue_size);

		/* Accusé de réception (pour la synchronisation) */
		recvClient(data->receiving_client.file_client_sock, buffer, 1);
		sendClient(data->sending_client.file_client_sock, buffer, 1);
	}

	*(data->thread_status) = -1;
	pthread_mutex_unlock(data->mutex_thread_status);
	printf("Le transfert s'est parfaitement déroulé !\n");
	pthread_exit(NULL);
}

struct Client newClient(int ssock, int *nb_c, int *max_fd)
{
	int msg_client_sock = 0;
	int file_client_sock = 0;
	struct sockaddr_in csin;
	socklen_t sinsize = 0; /* socket client */

	char username[16] = "";

	memset(&csin, 0, sizeof(csin));
	sinsize = sizeof(csin);
	msg_client_sock = accept(ssock, (struct sockaddr *)&csin, &sinsize); /* accepter la connexion du client pour les messsages */
	if (msg_client_sock == SOCKET_ERROR)
	{
		perror("accept error");
		exit(errno);
	}

	memset(&csin, 0, sizeof(csin));
	sinsize = sizeof(csin);
	file_client_sock = accept(ssock, (struct sockaddr *)&csin, &sinsize); /* accepter la connexion du client pour les fichiers */
	if (file_client_sock == SOCKET_ERROR)
	{
		perror("accept error");
		exit(errno);
	}

	*nb_c += 1;
	*max_fd = max(max(msg_client_sock, *max_fd), file_client_sock);

	if (recv(msg_client_sock, username, 16, 0) == SOCKET_ERROR)
	{
		perror("recv error");
		exit(errno);
	}

	username[15] = '\0';	// On s'assure que le username ne fasse pas plus de 16 caractères
	struct Client client = {"", 0, 0};
	client.msg_client_sock = msg_client_sock;
	client.file_client_sock = file_client_sock;
	strcpy(client.username, username);

	return client;
}

void rmvClient(struct Client *clients, int i_to_remove, int *nb_c, int *max_fd, int server_sock)
{
	int i = 0;

	close(clients[i_to_remove].msg_client_sock);
	close(clients[i_to_remove].file_client_sock);
	for (i = i_to_remove; i < *nb_c-1; i++)
	{
		clients[i] = clients[i+1];
	}

	*nb_c -= 1;
	*max_fd = server_sock;
	for (i = 0; i < *nb_c; i++)
	{
		*max_fd = max(max(clients[i].msg_client_sock, *max_fd), clients[i].file_client_sock);
	}
}

int recvClient(int sock, char *buffer, size_t buffer_size)
{
	ssize_t recv_outcome = 0;
	recv_outcome = recv(sock, buffer, buffer_size, 0);
	if (recv_outcome == SOCKET_ERROR)
	{
		perror("recv error");
		exit(errno);
	}
	else if (recv_outcome == 0)
	{
		return FALSE;
	}

	return TRUE;
}

int sendClient(int sock, char *buffer, size_t buffer_size)		// On pourra mettre strlen(buffer)+1 pour les chaînes de caractères
{
	if (send(sock, buffer, buffer_size, 0) == SOCKET_ERROR)
	{
		perror("send error");
		exit(errno);
	}

	return TRUE;
}

int max(int a, int b)
{
	return (a >= b) ? a : b;
}
