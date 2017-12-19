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

int answerSendingRequest(char *request, char *path)
{
	/* Cette fonction prend en paramètre une requête brute, un buffer pour intérroger le client et le path final de réception */
	/* Elle s'occupe de demander au client destinataire s'il souhaite recevoir le fichier */

	char buffer[2];
	char *file_name = NULL;
	char *char_ptr = NULL;
	FILE *f = NULL;
	int reset = 0;

	strcpy(path, "/home/clement/Reception/");
	file_name = strrchr(request, '/')+1;
	strcat(path, file_name);

	f = fopen(path, "r");
	if (f == NULL)
	{
		printf("Voulez-vous recevoir le fichier %s ?\n1 : Oui, 0 : Non\n", file_name);
		if (fgets(buffer, sizeof(buffer), stdin) == NULL)
		{
			perror("fgets error");
			exit(errno);
		}

		char_ptr = strchr(buffer, '\n');
		if (char_ptr != NULL)
		{
			*char_ptr = '\0';
		}
		else
		{
			while (reset != '\n' && reset != EOF)
			{
				reset = getchar();
			}
		}
		return atoi(buffer);
	}
	else
	{
		printf("Quelqu'un souhaite vous envoyer le fichier %s mais il existe déjà (%s)\n", file_name, path);
		fclose(f);
		return 0;
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
	/* Fonction gérant l'envoi du fichier */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";
	long int size = 0;
	int residue_size = 0;
	int package_number = 0;
	int i = 0;

	while(pthread_mutex_lock(data->mutex_thread_status) == EDEADLK)
	{
		continue;
	}

	FILE *f = NULL;

	f = fopen(data->path, "rb");
	if (f == NULL)
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);

		fclose(f);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		fprintf(stderr, "Envoi impossible : le chemin n'est pas valide ou le fichier est inexistant\n");
		pthread_exit(NULL);
	}

	fseek(f, 0, SEEK_END);

	size = ftell(f);
	package_number = size / BUFFER_SIZE;	// Pas de '\0' pour fermer le segment car il risque déjà d'y avoir des caractères NULL dans le buffer
	residue_size = size % BUFFER_SIZE;
	sprintf(buffer, "%d %d", package_number, residue_size);			// '\0' écrit par sprintf
	sendServer(data->file_server_sock, buffer, strlen(buffer)+1);
	recvServer(data->file_server_sock, buffer, 1);		// Accusé de réception pour la synchronisation

	fseek(f, 0, SEEK_SET);

	for (i = 0; i < package_number; i++)	// Paquets normaux (de 1024 octets)
	{
		fread(buffer, BUFFER_SIZE, 1, f);
		sendServer(data->file_server_sock, buffer, sizeof(buffer));

		/* Accusé de réception (pour la synchronisation) */
		recvServer(data->file_server_sock, buffer, 1);
	}
	if (residue_size != 0)					// Traitement du dernier paquet s'il n'est pas constitué de 1024 octets
	{
		fread(buffer, residue_size, 1, f);
		sendServer(data->file_server_sock, buffer, residue_size);

		/* Accusé de réception (pour la synchronisation) */
		recvServer(data->file_server_sock, buffer, 1);
	}

	fclose(f);
	*(data->thread_status) = -1;
	pthread_mutex_unlock(data->mutex_thread_status);
	printf("L'envoi s'est parfaitement déroulé !\n");
	pthread_exit(NULL);
}

void *transferRecvControl(void *src_data)
{
	/* Fonction gérant la réception du fichier */
	/* Attention ! path doit être l'endroit où l'on va enregistrer le fichier */
	/* D'ailleurs, le cas de préexistence de ce dernier doit déjà avoir été traité */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";
	char *char_ptr = NULL;
	int package_number = 0;
	int residue_size = 0;
	int i = 0;

	while(pthread_mutex_lock(data->mutex_thread_status) == EDEADLK)
	{
		continue;
	}

	FILE *f = NULL;
	printf("%s\n", data->path);
	f = fopen(data->path, "wb");
	if (f == NULL)
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);

		fclose(f);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		fprintf(stderr, "Réception impossible : le chemin n'est pas valide\n");
		pthread_exit(NULL);
	}

	recvServer(data->file_server_sock, buffer, sizeof(buffer));		// Réception du nombre de paquets et de la taille du dernier paquet
	char_ptr = strchr(buffer, ' ');
	*char_ptr = '\0';
	package_number = atoi(buffer);
	residue_size = atoi(char_ptr + 1);
	buffer[0] = -1;
	sendServer(data->file_server_sock, buffer, 1);		// Accusé de réception pour la synchronisation

	for (i = 0; i < package_number; i++)
	{
		recvServer(data->file_server_sock, buffer, sizeof(buffer));
		fwrite(buffer, sizeof(buffer), 1, f);

		buffer[0] = -1;
		/* Accusé de réception (pour la synchronisation) */
		sendServer(data->file_server_sock, buffer, 1);
	}
	if (residue_size != 0)
	{
		recvServer(data->file_server_sock, buffer, sizeof(buffer));
		fwrite(buffer, residue_size, 1, f);

		buffer[0] = -1;
		/* Accusé de réception (pour la synchronisation) */
		sendServer(data->file_server_sock, buffer, 1);
	}

	fclose(f);
	*(data->thread_status) = -1;
	pthread_mutex_unlock(data->mutex_thread_status);
	printf("La réception s'est parfaitement déroulée !\n");
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
	sin.sin_port = htons(port); /* le port +1 pour bien se connecter au socket du serveur destiné aux fichiers */
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

int sendServer(int sock, char *buffer, size_t buffer_size)	// On pourra utiliser strlen(buffer)+1 pour buffer_size si l'on envoie une chaîne de caractères
{
	if (send(sock, buffer, buffer_size, 0) == SOCKET_ERROR)
	{
		perror("send error");
		exit(errno);
	}

	return EXIT_SUCCESS;
}
