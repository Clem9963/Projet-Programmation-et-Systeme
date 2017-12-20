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
	int answer = 0;
	int thread_status = 0;	// 0 Si le thread n'est pas en cours, 1 s'il est en cours et -1 s'il attend que l'on lise son code de retour
	fd_set readfs;
	pthread_t file_transfer = 0;
	pthread_mutex_t mutex_thread_status = PTHREAD_MUTEX_INITIALIZER;
	struct TransferDetails data = {NULL, 0, 0, NULL, NULL};

	int reset = 0;
	char *char_ptr = NULL;

	char *username = NULL;
	char *address = NULL;
	int port = 0;

	if (argc != 4)
	{
		fprintf(stderr, "< FERROR > Nombre d'arguments fournis incorrect\n");
		exit(EXIT_FAILURE);
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
	sendServer(msg_server_sock, username, strlen(username)+1);
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
			perror("< FERROR > select error");
			exit(errno);
		}

		if (pthread_mutex_trylock(&mutex_thread_status) != EBUSY)
		{
			if (thread_status == -1)
			{
				pthread_join(file_transfer, NULL);
				thread_status = 0;
			}
			pthread_mutex_unlock(&mutex_thread_status);
		}

		if(FD_ISSET(msg_server_sock, &readfs))
		{
			/* Des données sont disponibles sur la socket du serveur */

			if (!recvServer(msg_server_sock, buffer, sizeof(buffer)))
			{
				printf("< FERROR > Le serveur n'est plus joignable\n");
				close(msg_server_sock);
				close(file_server_sock);
				exit(EXIT_SUCCESS);
			}
			else if (!strncmp(buffer, "/sendto", 7))		// ATTENTION ! strncmp renvoie 0 si les deux chaînes sont égales !
			{
				if (pthread_mutex_trylock(&mutex_thread_status) == EBUSY)
				{
					printf("< FTS > Un transfert est déjà en cours, patientez...\n");
					sprintf(buffer, "%d", 0);
					sendServer(file_server_sock, buffer, strlen(buffer)+1);
				}
				else
				{
					if (thread_status == 1)
					{
						printf("< FTS > Un transfert est déjà en cours, patientez...\n");
						sprintf(buffer, "%d", 0);
						sendServer(file_server_sock, buffer, strlen(buffer)+1);
					}
					else if (thread_status == -1)
					{
						pthread_join(file_transfer, NULL);
						thread_status = 0;
					}
					if (thread_status == 0)				// Pas de else car thread_status pourrait être modifié par le if précédent
					{
						answer = answerSendingRequest(buffer, path);
						sprintf(buffer, "%d", answer);
						sendServer(file_server_sock, buffer, strlen(buffer)+1);
						if (answer)
						{
							printf("< FTS > Vous avez accepté le transfert\n");
							data.path = path;
							data.file_server_sock = file_server_sock;
							data.msg_server_sock = msg_server_sock;
							data.thread_status = &thread_status;
							data.mutex_thread_status = &mutex_thread_status;

							thread_status = 1;
							if (pthread_create(&file_transfer, NULL, transferRecvControl, &data) != 0)
							{
								fprintf(stderr, "< FTS > Le démarrage de la réception a échoué\n");
								thread_status = 0;
							}
						}
						else
						{
							printf("< FTS > Le fichier ne sera pas reçu\n\n");
						}
					}
					pthread_mutex_unlock(&mutex_thread_status);
				}
			}
			else if (!strncmp(buffer, "/abort", 6))
			{
				fprintf(stderr, "< FERROR > Une erreur a eu lieu pendant le transfert\n");
				exit(EXIT_FAILURE);
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

			if (!strncmp(buffer, "/sendto", 7))
			{
				if (pthread_mutex_trylock(&mutex_thread_status) == EBUSY)
				{
					printf("< FTS > Un transfert est déjà en cours, patientez...\n");
				}		
				else
				{
					if (thread_status == 1)
					{
						printf("< FTS > Un transfert est déjà en cours, patientez...\n");
					}
					else if (thread_status == -1)
					{
						pthread_join(file_transfer, NULL);
						thread_status = 0;
					}
					if (thread_status == 0)				// Pas de else car thread_status pourrait être modifié par le if précédent
					{
						if (verifySendingRequest(buffer, dest_username, path) && verifyDirectory(path))
						{
							sendServer(msg_server_sock, buffer, strlen(buffer)+1);		// Envoi de la requête brute
							recvServer(file_server_sock, buffer, sizeof(buffer));
							answer = atoi(buffer);
							if (answer == -1)
							{
								fprintf(stderr, "< FTS > L'username n'est pas connu par le serveur\n");
							}
							else if (answer == -2)
							{
								printf("< FTS > Un transfert est déjà en cours, patientez...\n");
							}
							else if (answer == 0)
							{
								printf("< FTS > Le destinataire ne souhaite pas recevoir le fichier\n        Il a peut-être peur de vous...\n\n");
							}
							else
							{
								data.path = path;
								data.file_server_sock = file_server_sock;
								data.msg_server_sock = msg_server_sock;
								data.thread_status = &thread_status;
								data.mutex_thread_status = &mutex_thread_status;

								thread_status = 1;
								if (pthread_create(&file_transfer, NULL, transferSendControl, &data) != 0)
								{
									fprintf(stderr, "< FTS > Le démarrage de l'envoi a échoué\n");
									thread_status = 0;
								}
							}
						}
					}
					pthread_mutex_unlock(&mutex_thread_status);
				}
			}
			else if (!strcmp(buffer, "/abort"))
			{
				printf("Cette commande n'est pas autorisée\n");
			}
			else
			{
				/* On a affaire à un message standard */
				sendServer(msg_server_sock, buffer, strlen(buffer)+1);
			} 
		}
	}

	return EXIT_SUCCESS;
}
