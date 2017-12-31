#include "server.h"
#include "server_functions.h"

int main(int argc, char *argv[])
{
	/* La fonction main attend 1 paramètre :
	-> Le port du serveur */

	char buffer[BUFFER_SIZE] = "";									// Buffer de 1024 octets pour l'envoi et le réception des messages
	char formatting_buffer[FORMATTING_BUFFER_SIZE] = "";			// Buffer de 1048 octets pour le formatage du message avant la copie dans buffer, précédant l'envoi
	char request[BUFFER_SIZE] = "";									// Buffer de 1024 octets pour stocker la requête "/sendto"
	char *username;													// Pour récuperer le pseudo de l'utilisateur dans une commande "/kick"
	struct Client clients[MAX_CLIENTS];								// Tableau des clients actifs
	int clients_nb = 0;												// Nombre de clients connectés simultanément
	int index = 0;

	int port = 0;
	int passive_server_sock = 0;

	int selector = 0;
	int max_fd = 0;
	fd_set readfs;

	int thread_status = 0;			// 0 Si le thread n'est pas en cours, 1 s'il est en cours et -1 s'il attend que l'on lise son code de retour
	pthread_t file_transfer = 0;	// Thread relatif au transfert de fichiers
	pthread_mutex_t mutex_thread_status = PTHREAD_MUTEX_INITIALIZER;		// Initialisation du mutex (type "rapide") pour le vérouillage de "thread_status"
	struct TransferDetails data;											// Structure servant pour le passage d'arguments au thread

	int reset = 0;															// variable servant à flush le flux stdin
	char *char_ptr = NULL;													// variable servant à chercher un caractère dans un tableau de caractères

	int i = 0;
	int j = 0;
	int success = 0;														// Booléen servant dans des boucles while

	if (argc != 2)
	{
		fprintf(stderr, "< FERROR > Arguments fournis incorrects\n");
		exit(EXIT_FAILURE);
	}

	memset(&data, 0, sizeof(data));											// Initialisation de la structure en mettant tous les bits à 0

	port = atoi(argv[1]);

	passive_server_sock = listenSocket(port);		// Le serveur commence à écouter sur sa socket passive
	max_fd = passive_server_sock;

	printf("Le serveur est opérationnel\nEn attente de la connexion d'un client...\n");

	while(TRUE)										// Boucle du programme (écoute perpétuelle des sockets)
	{
		FD_ZERO(&readfs);
		FD_SET(passive_server_sock, &readfs);		// On va écouter la socket passive du serveur
		FD_SET(STDIN_FILENO, &readfs);				// On va écouter l'entrée standard
		for (i = 0; i < clients_nb; i++)
		{
			FD_SET(clients[i].msg_client_sock, &readfs);	// Pour chaque client, on écoute sa socket relative au messages
		}

		if((selector = select(max_fd + 1, &readfs, NULL, NULL, NULL)) < 0)
		{
			perror("< FERROR > select error");
			exit(errno);
		}

		/* Ci-après, on tente le vérouillage du mutex */

		if (pthread_mutex_trylock(&mutex_thread_status) != EBUSY)
		{
			/* On a réussi à vérouiller, on peut donc accéder à la variable "thread_status" */
			if (thread_status == -1)	// Si elle vaut -1, cela signifie que le thread est terminé et qu'il attend que l'on lise son code de retour
			{
				pthread_join(file_transfer, NULL);		// Lecture du code de retour
				thread_status = 0;						// Le thread est terminé
			}
			pthread_mutex_unlock(&mutex_thread_status);	// Déverouillage du mutex
		}
		
		if(FD_ISSET(passive_server_sock, &readfs))
		{
			/* Des données sont disponibles sur la socket passive du serveur */

			/* Même si clients_nb est modifié par la fonction, c'est toujours l'ancienne valeur qui est prise en compte lors de l'affectation suivante */
			clients[clients_nb] = newClient(passive_server_sock, &clients_nb, &max_fd);
			
			/* Vérification que le pseudo est différent des autres clients connectés */
			success = TRUE;	// Initialisation d'un booléen représentant le succès de la connexion du nouveau client
			for (i = 0; i < clients_nb-1 && success; i++)
			{
				if (!strcmp(clients[i].username, clients[clients_nb-1].username))
				{
					success = FALSE;
				}
			}
			if (!success)	// Si le pseudo existe déjà
			{
				sprintf(buffer, "%d", FALSE);
				sendClient(clients[clients_nb-1].msg_client_sock, buffer, strlen(buffer)+1);
				recvClient(clients[i].msg_client_sock, buffer, sizeof(buffer));					// Accusé de réception pour la synchronisation
				rmvClient(clients, clients_nb-1, &clients_nb, &max_fd, passive_server_sock);	// Déconnexion du client en question
			}
			if (success)	// Si le pseudo n'existe pas
			{
				sprintf(buffer, "%d", TRUE);
				sendClient(clients[clients_nb-1].msg_client_sock, buffer, strlen(buffer)+1);
				recvClient(clients[i].msg_client_sock, buffer, sizeof(buffer));				// Accusé de réception pour la synchronisation
				
				sprintf(formatting_buffer, "Bienvenue %s", clients[clients_nb-1].username);
				formatting_buffer[BUFFER_SIZE-1] = '\0';
				strcpy(buffer, formatting_buffer);
				sendClient(clients[clients_nb-1].msg_client_sock, buffer, strlen(buffer)+1);
				
				sprintf(formatting_buffer, "%s s'est connecté(e)", clients[clients_nb-1].username);
				formatting_buffer[BUFFER_SIZE-1] = '\0';
				printf("%s\n", formatting_buffer);
				sendToOther(clients, formatting_buffer, clients_nb-1, clients_nb);
			}
		}

		for (i = 0; i < clients_nb; i++)
		{
			if(FD_ISSET(clients[i].msg_client_sock, &readfs))
			{
				/* Des données sont disponibles sur la socket relative aux messages d'un des clients */

				if (!recvClient(clients[i].msg_client_sock, buffer, sizeof(buffer)))
				{
					/* Le client s'est déconnecté */
					strcpy(formatting_buffer, "Déconnexion de ");
					strcat(formatting_buffer, clients[i].username);
					formatting_buffer[BUFFER_SIZE-1] = '\0';
					strcpy(buffer, formatting_buffer);
					printf("%s\n", buffer);
					sendToOther(clients, buffer, i, clients_nb);
					rmvClient(clients, i, &clients_nb, &max_fd, passive_server_sock);
				}
				else if (!strncmp(buffer, "/sendto", 7))		// ATTENTION ! strncmp et strcmp renvoient 0 si les deux chaînes sont égales !
				{
					printf("< FTS > %s a envoyé une requête /sendto...\n", clients[i].username);
					if (pthread_mutex_trylock(&mutex_thread_status) == EBUSY)
					{
						/* La tentative de vérouillage du mutex a échoué */
						printf("< FTS > Un transfert est déjà en cours, requête annulée\n");
						sprintf(buffer, "%d", -2);
						sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);	// On pense tout de même à répondre au client émetteur
					}
					else
					{
						if (thread_status == 1)
						{
							printf("< FTS > Un transfert est déjà en cours, requête annulée\n");
							sprintf(buffer, "%d", -2);
							sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);
						}
						else if (thread_status == -1)	// Le thread attend que l'on lise son code de retour
						{
							pthread_join(file_transfer, NULL);
							thread_status = 0;
						}
						if (thread_status == 0)			// Pas de else car thread_status pourrait être modifié par le if précédent
						{
							index = getClient(clients, clients_nb, buffer);		// Récupération de l'indice du client destinataire
							if (index == -1)
							{
								/* On signale au client émetteur que le pseudo de la requête "/sendto" ne
								correspond pas à un client connecté */
								printf("< FTS > La requête est non conforme (nom du destinataire inconnu)\n");
								sprintf(buffer, "%d", index);
								sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);
							}
							else
							{
								strcpy(request, buffer);					// On utilise une variable annexe pour stocker la requête pendant le transfert

								/* Affectation de la structure à passer en argument par adresse au thread relatif au transfert */
								data.sending_client = clients[i];
								data.receiving_client = clients[index];
								data.request = request;
								data.thread_status = &thread_status;
								data.mutex_thread_status = &mutex_thread_status;

								thread_status = 1;					// thread_status change de valeur pour indiquer qu'un thread de transfert est en cours
								if (pthread_create(&file_transfer, NULL, transferControl, &data) != 0)
								{
									fprintf(stderr, "< FTS > Le démarrage du transfert a échoué\n");
									sprintf(buffer, "%d", -2);
									sendClient(clients[i].file_client_sock, buffer, strlen(buffer)+1);	// On pense tout de même à répondre au client émetteur
									thread_status = 0;
								}
							}
						}
						pthread_mutex_unlock(&mutex_thread_status);		// On n'oublie pas de déverouiller le mutex !
					}
				}
				else if(!strcmp(buffer, "/list"))		// Si la requête du client est "/list" on lui envoie la liste des clients connectés
				{
					printf("%s : /list\n", clients[i].username);	// Affichage d'une trace sur le terminal du serveur
					strcpy(formatting_buffer, "Les clients connectés sont : ");
					for (j = 0; j < clients_nb; j++)
					{
						if(j != i)
						{
							strcat(formatting_buffer, clients[j].username);
						}
						else
						{
							strcat(formatting_buffer, "vous");
						}
						if (j != clients_nb-1)
						{
							strcat(formatting_buffer, ", ");
						}
					}
					formatting_buffer[BUFFER_SIZE-1] = '\0';
					strcpy(buffer, formatting_buffer);
					sendClient(clients[i].msg_client_sock, buffer, strlen(buffer)+1);
				}
				else if (!strcmp(buffer, "/abort"))		// Si l'on reçoit une commande du type /abort
				{
					/* On n'a pas besoin de faire de test car le "/abort" ne peut être envoyé volontairement par l'utilisateur */
					/* En effet, seulement le thread relatif au transfert, s'il rencontre un problème peut être amené à générer une telle requête */

					if (!strcmp(clients[i].username, data.receiving_client.username))	// C'est le destinataire qui a rencontré un problème
					{
						sendClient(data.sending_client.msg_client_sock, buffer, strlen(buffer)+1);
					}
					else																// C'est l'émetteur qui a rencontré un problème
					{
						sendClient(data.receiving_client.msg_client_sock, buffer, strlen(buffer)+1);
					}
				}
				else	// Ce qui se trouvait dans le buffer du client est envoyé pour le chat s'il ne correpond à aucun des cas précedents, d'où le "else"
				{
					strcpy(formatting_buffer, "");
					strcpy(formatting_buffer, clients[i].username);
					strcat(formatting_buffer, " : ");
					strcat(formatting_buffer, buffer);
					formatting_buffer[BUFFER_SIZE-1] = '\0';
					strcpy(buffer, formatting_buffer);
					printf("%s\n", buffer);
					sendToOther(clients, buffer, i, clients_nb);
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

			/* Suppression du saut de ligne final */
			char_ptr = strchr(buffer, '\n');
			if (char_ptr != NULL)
			{
				*char_ptr = '\0';
			}

			/* On flush le flux stdin */
			else
			{
				while (reset != '\n' && reset != EOF)
				{
					reset = getchar();
				}
			}
			reset = 0;

			if (!strcmp(buffer, "/abort"))		// Si on entre une commande du type "/abort"	
			{
				if (pthread_mutex_trylock(&mutex_thread_status) != EBUSY)	// On tente de vérouiller le mutex
				{
					/* On a réussi à vérouiller donc aucun transfert n'est en cours */
					printf("< FTS > Aucun transfert n'est en cours, commande annulée\n\n");
					pthread_mutex_unlock(&mutex_thread_status);				// On n'oublie pas de déverouiller le mutex
				}
				else
				{
					sendClient(data.sending_client.msg_client_sock, buffer, strlen(buffer)+1);
					sendClient(data.receiving_client.msg_client_sock, buffer, strlen(buffer)+1);
					printf("< FTS > Transfert annulé avec succès !\n");
					printf("        Le serveur va s'arrêter !\n");
					sleep(1);

					/* On ferme toutes les sockets */
					close(passive_server_sock);
					for (i = 0; i < clients_nb; i++)
					{
						close(clients[i].msg_client_sock);
						close(clients[i].file_client_sock);
					}
					exit(EXIT_FAILURE);
					/* On ne peut pas kill le thread car ce dernier a vérouillé le mutex, on est donc obligé d'arrêter complétement le serveur */
				}
			}
			else if (!strcmp(buffer, "/quit"))		// Si on entre une commande du type "/quit"	
			{
				/* On ferme toutes les sockets */
				close(passive_server_sock);
				for (i = 0; i < clients_nb; i++)
				{
					close(clients[i].msg_client_sock);
					close(clients[i].file_client_sock);
				}

				printf("\n\nDéconnexion réussie\n");
				exit(EXIT_SUCCESS);
			}
			else if(!strncmp(buffer, "/kick", 5))	// Si on entre une commande du type "/kick <username>", on déconnecte le client demandé
			{
				/* Récupération du pseudo entré en deuxième paramètre */
				username = strtok(buffer, " ");
				username = strtok(NULL, " ");
				if (username != NULL)
				{	
					success = 0;
					username[USERNAME_SIZE - 1] = '\0';
					/* On cherche le client en comparant avec la liste des pseudos connus */
					for(i = 0; i < clients_nb; i++)
					{
						if(strcmp(clients[i].username, username) == 0)
						{
							rmvClient(clients, i, &clients_nb, &max_fd, passive_server_sock);
							printf("%s déconnecté(e) avec succès\n", username);
							sprintf(formatting_buffer, "%s a été déconnecté(e) \n", username);
							formatting_buffer[BUFFER_SIZE-1] = '\0';
							strcpy(buffer, formatting_buffer);
							sendToAll(clients, buffer, clients_nb);
							success = 1;
						}
					}

					if(!success)	// Vérification de la déconnexion effective du client
					{
						printf("Le client n'existe pas !\n");
					}
				}
				else
				{
					printf("Veuillez rentrer un pseudo après /kick!\n");
				}
			}
			else if(!strncmp(buffer, "/list", 5))	// Si on entre une commande de type "/list", on liste les clients connectés
			{
				printf("\nVoici les clients connectés :\n");
				for (i = 0; i < clients_nb; i++)
				{
					printf("%s\n", clients[i].username);
				}
			}
			else
			{
				strcpy(formatting_buffer, "SERVEUR : ");
				strcat(formatting_buffer, buffer);
				formatting_buffer[BUFFER_SIZE-1] = '\0';
				strcpy(buffer, formatting_buffer);
				printf("%s\n", buffer);
				sendToAll(clients, buffer, clients_nb);
			}
		}
	}

	return EXIT_SUCCESS;
}
