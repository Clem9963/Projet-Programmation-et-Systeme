#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client_functions.h"

#define h_addr h_addr_list[0]
#define SOCKET_ERROR -1			/* code d'erreur des sockets  */
#define TRUE 1
#define FALSE 0

void getDirectory(char *path, size_t length)
{
	FILE *f = NULL;
	int bouncer = 0;
	char *char_ptr = NULL;

	while (f == NULL)
	{
		printf("Veuillez entrer le chemin d'un fichier s'il-vous-plaît (relatif ou absolu)\n");
		if (fgets(path, length, stdin) == NULL)
		{
			perror("fgets error");
			exit(errno);
		}

		char_ptr = strchr(path, '\n');
		if (char_ptr != NULL)
		{
			*char_ptr = '\0';
		}
		else
		{
			while (bouncer != '\n' && bouncer != EOF)
			{
				bouncer = getchar();
			}
		}

		f = fopen(path, "r");
		if (f == NULL)
		{
			perror("fopen error");
			exit(errno);
		}
		else
		{
			printf("Fichier ouvert avec succès\n");
			fclose(f);
		}
	}
}

int connectSocket(char* address, int port)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent* hostinfo;
	struct sockaddr_in sin; /* structure qui possède toutes les infos pour le socket */

	if(sock == SOCKET_ERROR)
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

	printf("Connexion à %u.%u.%u.%u\n", hostinfo->h_addr[0], hostinfo->h_addr[1], hostinfo->h_addr[2], hostinfo->h_addr[3]);

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; /* on spécifie l'adresse */
	sin.sin_port = htons(port); /* le port */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */

	if (connect(sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR) /* demande de connexion */
	{
		perror("connect error");
		exit(errno);
	}

	return sock;
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
