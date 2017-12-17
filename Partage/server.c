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

int main(int argc, char *argv[])
{
	/* La fonction main attend 1 paramètre :
	-> Le port du serveur */

	char buffer[BUFFER_SIZE] = "";				// Buffer de 1024 octets pour l'envoi et le réception
	char formatting_buffer[FORMATTING_BUFFER_SIZE] = "";			// Buffer de 1048 octets pour le formatage du message avant la copie dans buffer
	struct Client clients[MAX_CLIENTS];
	int clients_nb = 0;

	int port = 0;
	int passive_server_sock = 0;

	int selector = 0;
	int max_fd = 0;
	fd_set readfs;

	int reset = 0;
	char *char_ptr = NULL;

	int i = 0;
	int j = 0;

	if (argc != 2)
	{
		fprintf(stderr, "Arguments fournis incorrects\n");
		return EXIT_FAILURE;
	}

	port = atoi(argv[1]);

	passive_server_sock = listenSocket(port);
	max_fd = passive_server_sock;

	while(TRUE)
	{
		FD_ZERO(&readfs);
		FD_SET(passive_server_sock, &readfs);
		FD_SET(STDIN_FILENO, &readfs);
		for (i = 0; i < clients_nb; i++)
		{
			FD_SET(clients[i].msg_client_sock, &readfs);
		}

		if((selector = select(max_fd + 1, &readfs, NULL, NULL, NULL)) < 0)
		{
			perror("select error");
			exit(errno);
		}
		
		if(FD_ISSET(passive_server_sock, &readfs))
		{
			/* Des données sont disponibles sur le socket du serveur */

			/* Même si clients_nb et modifié par la fonction, c'est toujours l'ancienne valeur qui est prise en compte lors de l'affectation */
			clients[clients_nb] = newClient(passive_server_sock, &clients_nb, &max_fd);
			strcpy(formatting_buffer, "Connexion de ");
			strcat(formatting_buffer, clients[clients_nb-1].username);
			formatting_buffer[BUFFER_SIZE-1] = '\0';
			strcpy(buffer, formatting_buffer);
			printf("%s\n", buffer);
			for (j = 0; j < clients_nb; j++)
			{	
				sendClient(clients[j].msg_client_sock, buffer, strlen(buffer)+1);
			}
		}

		for (i = 0; i < clients_nb; i++)
		{
			if(FD_ISSET(clients[i].msg_client_sock, &readfs))
			{
				/* Des données sont disponibles sur un des sockets clients */

				if (!recvClient(clients[i].msg_client_sock, buffer, sizeof(buffer)))
				{
					strcpy(formatting_buffer, "Déconnexion de ");
					strcat(formatting_buffer, clients[i].username);
					formatting_buffer[BUFFER_SIZE-1] = '\0';
					strcpy(buffer, formatting_buffer);
					printf("%s\n", buffer);
					for (j = 0; j < clients_nb; j++)
					{	
						sendClient(clients[j].msg_client_sock, buffer, strlen(buffer)+1);
					}
					rmvClient(clients, i, &clients_nb, &max_fd, passive_server_sock);
				}
				else	// Ce qui se trouvait dans le buffer du client est tout de même envoyé, d'où le "else"
				{
					strcpy(formatting_buffer, clients[i].username);
					strcat(formatting_buffer, " : ");
					strcat(formatting_buffer, buffer);
					formatting_buffer[BUFFER_SIZE-1] = '\0';
					strcpy(buffer, formatting_buffer);
					printf("%s\n", buffer);
					for (j = 0; j < clients_nb; j++)
					{	
						sendClient(clients[j].msg_client_sock, buffer, strlen(buffer)+1);
					}
				}
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
			// strcpy(formatting_buffer, "SERVEUR : ");
			// strcat(formatting_buffer, buffer);
			// formatting_buffer[BUFFER_SIZE-1] = '\0';
			// strcpy(buffer, formatting_buffer);
			printf("%s\n", buffer);
			for (i = 0; i < clients_nb; i++)
			{	
				sendClient(clients[i].msg_client_sock, buffer, strlen(buffer)+1);
			}
		}
	}

	return EXIT_SUCCESS;
}