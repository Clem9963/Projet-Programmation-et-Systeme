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

#define TRUE 1
#define FALSE 0

int main(int argc, char *argv[])
{
	/* La fonction main attend 3 paramètres :
	-> Le pseudo de l'utilisateur
	-> L'adresse IP du serveur
	-> Le port du serveur */

	char buffer[1024] = "";		// Buffer de 1024 octets pour l'envoi ou la réception de paquets à travers le réseau
	char path[512] = "";

	int server_sock = 0;
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
	if (strlen(username) > 15)
	{
		username[15] = '\0';	// Pas de pseudo trop long !
	}
	address = argv[2];
	port = atoi(argv[3]);

	server_sock = connectSocket(address, port);

	sendServer(server_sock, username);

	/* Traitement des requêtes */

	while(TRUE)
	{
		FD_ZERO(&readfs);
		FD_SET(server_sock, &readfs);
		FD_SET(STDIN_FILENO, &readfs);

		if((selector = select(server_sock + 1, &readfs, NULL, NULL, NULL)) < 0)
		{
			perror("select error");
			exit(errno);
		}
		
		if(FD_ISSET(server_sock, &readfs))
		{
			/* Des données sont disponibles sur la socket du serveur */

			if (!recvServer(server_sock, buffer, sizeof(buffer)))
			{
				printf("Le serveur n'est plus joignable\n");
				close(server_sock);
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
				printf("Rappel de syntaxe : \"/sendto <dest_username>\n");
			}
			else
			{
				sendServer(server_sock, buffer);					// Envoi de la requête de transfert
				recvServer(server_sock, buffer, sizeof(buffer));	// Réception d'un octet représentant la vérification du serveur -> 1 si OK, 0 sinon
			}
		}
	}

	return EXIT_SUCCESS;
}
