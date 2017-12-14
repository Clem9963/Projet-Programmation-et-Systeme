#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "client_functions.h"

int main(int argc, char *argv[])
{
	/* La fonction main attend 3 paramètres :
	-> Le pseudo de l'utilisateur
	-> L'adresse IP du serveur
	-> Le port du serveur */

	char buffer[BUFFER_SIZE] = "";		// Buffer de 1024 octets pour l'envoi ou la réception de paquets à travers le réseau
	char path[PATH_SIZE] = "";
	char dest_username[USERNAME_SIZE] = "";

	int msg_server_sock = 0;
	int file_server_sock = 0;
	int max_fd = 0;
	int selector = 0;
	fd_set readfs;

	int reset = 0;
	char *char_ptr = NULL;

	char *username = NULL;
	char *address = NULL;
	int port = 0;

	if (argc != 4)
	{
		fprintf(stderr, "Nombre d'arguments fournis incorrect\n");
	}

	/* Initialisation de la connexion */

	username = argv[1];
	if (strlen(username) > USERNAME_SIZE-1)
	{
		username[USERNAME_SIZE-1] = '\0';	// Pas de pseudo trop long !
	}
	address = argv[2];
	port = atoi(argv[3]);

	connectSocket(address, port, &msg_server_sock, &file_server_sock);
	sendServer(msg_server_sock, username);
	max_fd = (msg_server_sock >= file_server_sock) ? msg_server_sock : file_server_sock;

	/* Traitement des requêtes */

	while(TRUE)
	{
		FD_ZERO(&readfs);
		FD_SET(msg_server_sock, &readfs);
		FD_SET(file_server_sock, &readfs);
		FD_SET(STDIN_FILENO, &readfs);

		if((selector = select(max_fd + 1, &readfs, NULL, NULL, NULL)) < 0)
		{
			perror("select error");
			exit(errno);
		}
		
		if(FD_ISSET(msg_server_sock, &readfs))
		{
			/* Des données sont disponibles sur la socket du serveur */

			if (!recvServer(msg_server_sock, buffer, sizeof(buffer)))
			{
				printf("Le serveur n'est plus joignable\n");
				close(msg_server_sock);
				exit(EXIT_SUCCESS);
			}
			else
			{
				printf("%s\n", buffer);
			}
		}

		if(FD_ISSET(STDIN_FILENO, &readfs))
		{
			/* Des données sont disponibles sur l'entrée standard */

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

			if (strncmp(buffer, "/sendto", 7))
			{
				/* Le buffer ne commence pas par /sendto -> message normal */
				sendServer(msg_server_sock, buffer);
			}
			else
			{
				if (verifySendingRequest(buffer, dest_username, path))
				{
					if (verifyDirectory(path))
					{
						sendServer(file_server_sock, buffer);
					}
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
