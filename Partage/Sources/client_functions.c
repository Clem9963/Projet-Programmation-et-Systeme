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

#include "client.h"
#include "client_functions.h"

int verifyDirectory(char *path)
{
	/* Cette fonction vérifie si le chemin "path" est valide */
	FILE *f = NULL;

	f = fopen(path, "r");
	if (f == NULL)
	{
		fprintf(stderr, "Le chemin n'est pas valide ou le fichier est inexistant\n");
		return FALSE;
	}
	else
	{
		fclose(f);
		return TRUE;
	}
}

int verifySendingRequest(char *buffer, char *dest_username, char *path)
{
	/* Fonction vérifiant l'intégrité et la validité d'une requête /sendto 	*/
	/* Les tableaux dest_username et path sont remplis en fonction 			*/
	int i = 0;
	char *char_ptr = NULL;

	char_ptr = strchr(buffer, ' ');
	if (char_ptr != NULL)
	{
		i = 0;
		for (char_ptr += 1; char_ptr < buffer + strlen(buffer) && *char_ptr != ' '; char_ptr++)
		{
			dest_username[i] = *char_ptr;
			i++;
		}

		i = 0;
		for (char_ptr += 1; char_ptr < buffer + strlen(buffer) && *char_ptr != ' '; char_ptr++)
		{
			path[i] = *char_ptr;
			i++;
		}
	}
	else
	{
		fprintf(stderr, "Rappel de syntaxe : \"/sendto <dest_username> <path>\"\n");
		return FALSE;
	}

	if (path[0] == '\0')
	{
		fprintf(stderr, "Rappel de syntaxe : \"/sendto <dest_username> <path>\"\n");
		return FALSE;
	}

	return TRUE;
}

void *transferSendControl(void *src_data)
{
	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";
	long int size = 0;
	int size_residue = 0;
	int package_number = 0;
	int i = 0;

	while(pthread_mutex_lock(data->mutex_thread_status) == EDEADLK)
	{
		continue;
	}

	FILE *f = NULL;

	f = fopen(data->path, "r");
	if (f == NULL)
	{
		fprintf(stderr, "Envoi impossible : le chemin n'est pas valide ou le fichier est inexistant\n");
		buffer[0] = -1;							// On prévient le serveur que l'on annule le transfert
		buffer[0] = '\0';
		sendServer(data->file_server_sock, buffer);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		pthread_exit(NULL);
	}

	fseek(f, 0, SEEK_END);

	size = ftell(f);
	package_number = size / BUFFER_SIZE-1;	// Seulement BUFFER_SIZE-1 car un '\0' est rajouté en fin de paquet pour faciliter la segmentation
	size_residue = size % BUFFER_SIZE-1;
	sprintf(buffer, "%d", package_number + (size_residue != 0));		// '\0' écrit par sprintf
	sendServer(data->file_server_sock, buffer);

	fseek(f, 0, SEEK_SET);

	buffer[BUFFER_SIZE-1] = '\0';
	for (i = 0; i < package_number; i++)
	{
		fread(buffer, BUFFER_SIZE-1, 1, f);
		sendServer(data->file_server_sock, buffer);
	}
	if (size_residue != 0)
	{
		fread(buffer, size_residue, 1, f);
		buffer[size_residue] = '\0';
		sendServer(data->file_server_sock, buffer);
	}

	buffer[0] = -1;							// On met tous les bits à 1 pour le premier octet = signature de fin de transmission
	buffer[1] = '\0';						// Sinon on risque d'envoyer d'autres octets totalement inutiles
	sendServer(data->file_server_sock, buffer);

	*(data->thread_status) = -1;
	pthread_mutex_unlock(data->mutex_thread_status);
	pthread_exit(NULL);
}

void connectSocket(char* address, int port, int *msg_server_sock, int *file_server_sock)
{
	*msg_server_sock = socket(AF_INET, SOCK_STREAM, 0);
	*file_server_sock = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent* hostinfo;
	struct sockaddr_in sin; /* structure qui possède toutes les infos pour le socket */

	/* Connexion du socket pour envoyer des messages */

	if(*msg_server_sock == SOCKET_ERROR)
	{
		perror("socket error");
		exit(errno);
	}

	hostinfo = gethostbyname(address); /* infos du serveur */

	if (hostinfo == NULL) /* gethostbyname n'a pas trouvé le serveur */
	{
		herror("gethostbyname error");
        exit(errno);
	}

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; /* on spécifie l'adresse */
	sin.sin_port = htons(port); /* le port */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */

	if (connect(*msg_server_sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR) /* demande de connexion */
	{
		perror("connect error");
		exit(errno);
	}

	/* Connexion du socket pour envoyer des fichiers */

	if(*file_server_sock == SOCKET_ERROR)
	{
		perror("socket error");
		exit(errno);
	}

	hostinfo = gethostbyname(address); /* infos du serveur */

	if (hostinfo == NULL) /* gethostbyname n'a pas trouvé le serveur */
	{
		herror("gethostbyname");
        exit(errno);
	}

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; /* on spécifie l'adresse */
	sin.sin_port = htons(port+1); /* le port +1 pour bien se connecter au socket du serveur destiné aux fichiers */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */

	if (connect(*file_server_sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR) /* demande de connexion */
	{
		perror("connect error");
		exit(errno);
	}

	printf("Connexion à %u.%u.%u.%u\n", hostinfo->h_addr[0], hostinfo->h_addr[1], hostinfo->h_addr[2], hostinfo->h_addr[3]);
}

int recvServer(int sock, char *buffer, size_t buffer_size)
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

int sendServer(int sock, char *buffer)
{
	if (send(sock, buffer, strlen(buffer)+1, 0) == SOCKET_ERROR) 	// Le +1 représente le caractère nul
	{
		perror("send error");
		exit(errno);
	}

	return EXIT_SUCCESS;
}
