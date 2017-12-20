#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
		fprintf(stderr, "< FTS > Le chemin n'est pas valide ou le fichier est inexistant\n");
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

	const char* home_directory = getenv("HOME");
	char buffer[4];
	char *file_name = NULL;
	char *char_ptr = NULL;
	FILE *f = NULL;
	int reset = 0;
	int answer = -1;
	
	strcpy(path, home_directory);
	strcat(path, "/File_Transfer/");
	if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
	{
		if (errno != EEXIST)			// errno vaut EEXIST si et seulement si le dossier existe déjà
		{
			perror("mkdir error");
			return 0;
		}
	} 
	file_name = strrchr(request, '/')+1;
	strcat(path, file_name);

	f = fopen(path, "r");
	if (f == NULL)
	{
		printf("< FTS > Voulez-vous recevoir le fichier %s ?\n        1 : Oui, 0 : Non\n", file_name);
		while (answer != 0 && answer != 1)
		{
			if (fgets(buffer, sizeof(buffer), stdin) == NULL)
			{
				perror("fgets error");
				return 0;
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
			reset = 0;			// Si on repasse dans la boucle, reset doit être différent de EOF ou '\n' pour pouvoir flush le flux stdin

			answer = atoi(buffer);
			
			if (answer != 0 && answer != 1)
			{
				printf("< FTS > Vous n'avez pas saisi un nombre valide !\n");
			}
		}

		return answer;
	}
	else
	{
		printf("< FTS > Quelqu'un souhaite vous envoyer le fichier %s mais il existe déjà\n        (%s)\n", file_name, path);
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
		for (char_ptr += 1; char_ptr < buffer + strlen(buffer); char_ptr++)
		{
			path[i] = *char_ptr;
			i++;
		}
	}
	else
	{
		fprintf(stderr, "< FTS > Rappel de syntaxe : \"/sendto <dest_username> <path>\"\n");
		return FALSE;
	}

	if (path[0] == '\0')
	{
		fprintf(stderr, "< FTS > Rappel de syntaxe : \"/sendto <dest_username> <path>\"\n");
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
		fprintf(stderr, "< FTS > Envoi impossible : le chemin n'est pas valide ou le fichier est inexistant\n");
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
		if (i == package_number / 4 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 25%%\n");
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 50%%\n");
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 75%%\n");
		}
		
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
	printf("< FTS > L'envoi s'est parfaitement déroulé !\n\n");
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
	printf("< FTS > Réception du fichier dans %s\n", data->path);
	f = fopen(data->path, "wb");
	if (f == NULL)
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);

		fclose(f);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		fprintf(stderr, "< FTS > Réception impossible : le chemin n'est pas valide\n");
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
		if (i == package_number / 4 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 25%%\n");
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 50%%\n");
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 75%%\n");
		}
		
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
	printf("< FTS > La réception s'est parfaitement déroulée !\n\n");
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
		perror("< FERROR > socket error");
		exit(errno);
	}

	hostinfo = gethostbyname(address); /* infos du serveur */

	if (hostinfo == NULL) /* gethostbyname n'a pas trouvé le serveur */
	{
		herror("< FERROR > gethostbyname error");
        exit(errno);
	}

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; /* on spécifie l'adresse */
	sin.sin_port = htons(port); /* le port */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */

	if (connect(*msg_server_sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR) /* demande de connexion */
	{
		perror("< FERROR > connect error");
		exit(errno);
	}

	/* Connexion du socket pour envoyer des fichiers */

	if(*file_server_sock == SOCKET_ERROR)
	{
		perror("< FERROR > socket error");
		exit(errno);
	}

	hostinfo = gethostbyname(address); /* infos du serveur */

	if (hostinfo == NULL) /* gethostbyname n'a pas trouvé le serveur */
	{
		herror("< FERROR > gethostbyname");
        exit(errno);
	}

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; /* on spécifie l'adresse */
	sin.sin_port = htons(port); /* le port +1 pour bien se connecter au socket du serveur destiné aux fichiers */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */

	if (connect(*file_server_sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR) /* demande de connexion */
	{
		perror("< FERROR > connect error");
		exit(errno);
	}

	printf("Connexion à %d.%d.%d.%d\n", (hostinfo->h_addr[0]+256)%256, (hostinfo->h_addr[1]+256)%256, (hostinfo->h_addr[2]+256)%256, (hostinfo->h_addr[3]+256)%256);
}

int recvServer(int sock, char *buffer, size_t buffer_size)
{
	ssize_t recv_outcome = 0;
	recv_outcome = recv(sock, buffer, buffer_size, 0);
	if (recv_outcome == SOCKET_ERROR)
	{
		perror("< FERROR > recv error");
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
		perror("< FERROR > send error");
		exit(errno);
	}

	return EXIT_SUCCESS;
}
