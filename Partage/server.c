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

	char buffer[BUFFER_SIZE] = "";									// Buffer de 1024 octets pour l'envoi et le réception
	char formatting_buffer[FORMATTING_BUFFER_SIZE] = "";			// Buffer de 1048 octets pour le formatage du message avant la copie dans buffer
	char request[BUFFER_SIZE] = "";									// Buffer de 1024 octets pour stocker la requête /sendto
	struct Client clients[MAX_CLIENTS];
	int clients_nb = 0;
	int index = 0;

	int port = 0;
	int passive_server_sock = 0;

	int selector = 0;
	int max_fd = 0;
	fd_set readfs;

	int thread_status = 0;	// 0 Si le thread n'est pas en cours, 1 s'il est en cours et -1 s'il attend que l'on lise son code de retour
	pthread_t file_transfer = 0;
	pthread_mutex_t mutex_thread_status = PTHREAD_MUTEX_INITIALIZER;	 // Initialisation du mutex (type "rapide")
	struct TransferDetails data;

	int reset = 0;
	char *char_ptr = NULL;

	int i = 0;
	int j = 0;

	if (argc != 2)
	{
		fprintf(stderr, "< FERROR > Arguments fournis incorrects\n");
		exit(EXIT_FAILURE);
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
			perror("< FERROR > select error");
			exit(errno);
		}

		if (pthread_mutex_lock(&mutex_thread_status) != EDEADLK)
		{
			if (thread_status == -1)
			{
				pthread_join(file_transfer, NULL);
				thread_status = 0;
			}
			pthread_mutex_unlock(&mutex_thread_status);
		}
		
		if(FD_ISSET(passive_server_sock, &readfs))
		{
			/* Des données sont disponibles sur la socket du serveur */

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
				/* Des données sont disponibles sur une des sockets clients */

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
				else if (!strncmp(buffer, "/sendto", 7))		// ATTENTION ! strncmp renvoie 0 si les deux chaînes sont égales !
				{
					if (pthread_mutex_lock(&mutex_thread_status) == EDEADLK)
					{
						printf("< FTS > Un transfert est déjà en cours, patientez...\n");
						sprintf(buffer, "%d", -2);
						sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);
					}
					else
					{
						if (thread_status == 1)
						{
							printf("< FTS > Un transfert est déjà en cours, patientez...\n");
							sprintf(buffer, "%d", -2);
							sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);
						}
						else if (thread_status == -1)
						{
							pthread_join(file_transfer, NULL);
							thread_status = 0;
						}
						if (thread_status == 0)				// Pas de else car thread_status pourrait être modifié par le if précédent
						{
							index = getClient(clients, clients_nb, buffer);
							if (index == -1)
							{
								sprintf(buffer, "%d", index);
								sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);
							}
							else
							{
								strcpy(request, buffer);
								data.sending_client = clients[i];
								data.receiving_client = clients[index];
								data.request = request;
								data.thread_status = &thread_status;
								data.mutex_thread_status = &mutex_thread_status;

								thread_status = 1;
								if (pthread_create(&file_transfer, NULL, transferControl, &data) != 0)
								{
									fprintf(stderr, "< FTS > Le du transfert a échoué\n");
									thread_status = 0;
									//todo envoyer un abort au client expéditeur
								}
							}
						}
						pthread_mutex_unlock(&mutex_thread_status);
					}
				}
				else if (!strcmp(buffer, "/abort"))		// "Si l'on reçoit une commande du type /abort"	
				{
					/* On n'a pas besoin de faire de test car le /abort ne peut être envoyé par l'utilisateur */
					/* En effet, seulement le thread relatif au transfert, s'il rencontre un problème peut être amené à générer une telle requête */

					if (!strcmp(clients[i].username, data.receiving_client.username))
					{
						sendClient(data.sending_client.msg_client_sock, buffer, strlen(buffer)+1);
					}
					else
					{
						sendClient(data.receiving_client.msg_client_sock, buffer, strlen(buffer)+1);
					}
				}
				else	// Ce qui se trouvait dans le buffer du client est tout de même envoyé pour le chat, d'où le "else"
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
				perror("< FERROR > fgets error");
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

			if (!strcmp(buffer, "/abort"))		// "Si l'utilisateur entre une commande du type /abort"	
			{
				if (pthread_mutex_lock(&mutex_thread_status) != EDEADLK)
				{
					fprintf(stderr, "< FTS > Aucun transfert n'est en cours, commande annulée\n\n");
					pthread_mutex_unlock(&mutex_thread_status);
				}
				else
				{
					sendClient(data.sending_client.msg_client_sock, buffer, strlen(buffer)+1);
					sendClient(data.receiving_client.msg_client_sock, buffer, strlen(buffer)+1);
					printf("< FTS > Transfert annulé avec succès !\n");
				}
			}
			else
			{
				strcpy(formatting_buffer, "SERVEUR : ");
				strcat(formatting_buffer, buffer);
				formatting_buffer[BUFFER_SIZE-1] = '\0';
				strcpy(buffer, formatting_buffer);
				printf("%s\n", buffer);
				for (i = 0; i < clients_nb; i++)
				{	
					sendClient(clients[i].msg_client_sock, buffer, strlen(buffer)+1);
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
