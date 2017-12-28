#include <errno.h>
#include <ncurses.h>
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

	char buffer[BUFFER_SIZE] = "";			// Buffer de 1024 octets pour l'envoi ou la réception de paquets à travers le réseau
	char msg_buffer[BUFFER_SIZE] = "";	// Buffer de 1024 octets pour les messages qu'entre l'utilisateur caractère parcaractère
	char path[PATH_SIZE] = "";
	char dest_username[USERNAME_SIZE] = "";
	WINDOW *top_win, *bottom_win;
	int line = 0, letter, msg_len = 0;

	int msg_server_sock = 0;
	int file_server_sock = 0;
	int max_fd = 0;
	int selector = 0;
	int answer = 0;
	int thread_status = 0;	// 0 Si le thread n'est pas en cours, 1 s'il est en cours et -1 s'il attend que l'on lise son code de retour
	fd_set readfs;
	pthread_t file_transfer = 0;
	pthread_mutex_t mutex_thread_status = PTHREAD_MUTEX_INITIALIZER;
	struct TransferDetails data;

	int i = 0;

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
	recvServer(msg_server_sock, buffer, sizeof(buffer));		// Réponse du serveur quant à la disponibilité du pseudo
	if (!atoi(buffer))											// Si la réponse est fausse, le pseudo est déjà utilisé
	{
		fprintf(stderr, "< FERROR > Le pseudonyme est déjà utilisé par un autre utilisateur\n");
		close(msg_server_sock);
		close(file_server_sock);
		exit(-1);
	}
	buffer[0] = -1;												// Accusé de réception (pour la synchronisation)
	sendServer(msg_server_sock, buffer, 1);
	max_fd = (msg_server_sock >= file_server_sock) ? msg_server_sock : file_server_sock;

	//Initialisation de l'interface graphique
	initscr();
	keypad(stdscr, TRUE); //pour récupérer les touches spéciales du clavier
	top_win = subwin(stdscr, LINES - 3, COLS, 0, 0);
    bottom_win = subwin(stdscr, 3, COLS, LINES - 3, 0);
	initInterface(top_win, bottom_win);

	//creation de la conversation (je suis obligé de faire l'initialisation de la conversation à cet endroit 
    //car il faut la faire après initscr() pour récupérer le "LINES")
	char **conversation = calloc(LINES - 6, sizeof(char *));
	for (i = 0; i < LINES - 6; i++)
	{
		conversation[i] = calloc(BUFFER_SIZE, sizeof(char));
	}

	//se positionn au bon endroit pour ecrire le message
	move(LINES - 2, 4);

	/* Traitement des requêtes */

	while(TRUE)
	{
		FD_ZERO(&readfs);
		FD_SET(msg_server_sock, &readfs);
		FD_SET(file_server_sock, &readfs);
		FD_SET(STDIN_FILENO, &readfs);

		if((selector = select(max_fd + 1, &readfs, NULL, NULL, NULL)) < 0)
		{
			endwin();
			perror("< FERROR > select error");
			clearMemory(msg_server_sock, file_server_sock, conversation, top_win, bottom_win);
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
				clearMemory(msg_server_sock, file_server_sock, conversation, top_win, bottom_win);
				endwin();
				printf("< FERROR > Le serveur n'est plus joignable\n");
				exit(EXIT_SUCCESS);
			}
			else if (!strncmp(buffer, "/sendto", 7))		// ATTENTION ! strncmp renvoie 0 si les deux chaînes sont égales !
			{
				if (pthread_mutex_trylock(&mutex_thread_status) == EBUSY)
				{
					writeInConv("< FTS > Un transfert est déjà en cours, patientez...", conversation, &line, top_win, bottom_win);
					sprintf(buffer, "%d", 0);
					sendServer(file_server_sock, buffer, strlen(buffer)+1);
				}
				else
				{
					if (thread_status == 1)
					{
						writeInConv("< FTS > Un transfert est déjà en cours, patientez...", conversation, &line, top_win, bottom_win);
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
						answer = answerSendingRequest(buffer, path, conversation, &line, top_win, bottom_win);
						sprintf(buffer, "%d", answer);
						sendServer(file_server_sock, buffer, strlen(buffer)+1);
						if (answer)
						{
							writeInConv("< FTS > Vous avez accepté le transfert", conversation, &line, top_win, bottom_win);
							data.path = path;
							data.file_server_sock = file_server_sock;
							data.msg_server_sock = msg_server_sock;
							data.thread_status = &thread_status;
							data.mutex_thread_status = &mutex_thread_status;
							data.conversation = conversation;
							data.line = &line;
							data.top_win = top_win;
							data.bottom_win = bottom_win;

							thread_status = 1;
							if (pthread_create(&file_transfer, NULL, transferRecvControl, &data) != 0)
							{
								writeInConv("< FTS > Le démarrage de la réception a échoué", conversation, &line, top_win, bottom_win);
								thread_status = 0;
							}
						}
						else
						{
							writeInConv("< FTS > Le fichier ne sera pas reçu", conversation, &line, top_win, bottom_win);
						}
					}
					pthread_mutex_unlock(&mutex_thread_status);
				}
			}
			else if (!strncmp(buffer, "/abort", 6))
			{
				clearMemory(msg_server_sock, file_server_sock, conversation, top_win, bottom_win);
				endwin();
				fprintf(stderr, "< FERROR > Une erreur a eu lieu pendant le transfert\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				writeInConv(buffer, conversation, &line, top_win, bottom_win);
			}
		}

		if(FD_ISSET(STDIN_FILENO, &readfs))
		{
			/* Des données sont disponibles sur l'entrée standard */

			//récupère la lettre entrée 
			letter = getch();

			//si on appuie sur la touche flèche de gauche
			if (letter == 260 && msg_len > 0)
			{
				msg_len--;
				move(LINES - 2, 4 + msg_len);
			}
			//si on appuie sur la touche flèche de droite
			else if (letter == 261 && msg_len < COLS - 5)
			{
				msg_len++;
				move(LINES - 2, 4 + msg_len);
			}
			//si ce n'est pas la touche entrée et que l'on n'a pas remplit la ligne
			//10 = touche entrée
			else if(letter != 10 && letter != 263 && msg_len < COLS - 5)
			{
				//on met la lettre dans le bufferMessagEntre
				msg_buffer[msg_len] = letter;
				msg_len++;
			}
			//si on appuie sur la touche retour pour effacer un caractère
			else if (letter == 263 && msg_len > 0)
			{
				msg_len--;
				msg_buffer[msg_len] = ' ';
				mvwprintw(bottom_win, 1, 4 + msg_len, " ");
				move(LINES - 2, 4 + msg_len);
				wrefresh(bottom_win);
			}
			else
			{
				//on met le buffer
				msg_buffer[msg_len] = '\0';
				msg_len = 0;

				//se déconnecte du chat si le message est "/quit"
				if(!strcmp(msg_buffer, "/quit"))
				{
					clearMemory(msg_server_sock, file_server_sock, conversation, top_win, bottom_win);
					endwin();
					printf("\nDeconnexion réussie\n\n");
					exit(EXIT_SUCCESS);
				}

				else if (!strcmp(msg_buffer, "/abort"))
				{
					writeInConv("< ERROR > Cette commande n'est pas autorisée", conversation, &line, top_win, bottom_win);
				}

				else if (!strncmp(msg_buffer, "/sendto", 7))
				{
					if (pthread_mutex_trylock(&mutex_thread_status) == EBUSY)
					{
						writeInConv("< FTS > Un transfert est déjà en cours, patientez...", conversation, &line, top_win, bottom_win);
					}		
					else
					{
						if (thread_status == 1)
						{
							writeInConv("< FTS > Un transfert est déjà en cours, patientez...", conversation, &line, top_win, bottom_win);
						}
						else if (thread_status == -1)
						{
							pthread_join(file_transfer, NULL);
							thread_status = 0;
						}
						if (thread_status == 0)				// Pas de else car thread_status pourrait être modifié par le if précédent
						{
							if (verifySendingRequest(msg_buffer, dest_username, path, conversation, &line, top_win, bottom_win)
								&& verifyDirectory(path, conversation, &line, top_win, bottom_win))
							{
								writeInConv("< FTS > Envoi de la requête en cours. En attente d'une réponse...", conversation, &line, top_win, bottom_win);
								sendServer(msg_server_sock, msg_buffer, strlen(msg_buffer)+1);		// Envoi de la requête brute
								recvServer(file_server_sock, buffer, sizeof(buffer));
								answer = atoi(buffer);
								if (answer == -1)
								{
									writeInConv("< FTS > L'username n'est pas connu par le serveur", conversation, &line, top_win, bottom_win);
								}
								else if (answer == -2)
								{
									writeInConv("< FTS > Un transfert est déjà en cours, patientez...", conversation, &line, top_win, bottom_win);
								}
								else if (answer == 0)
								{
									writeInConv("< FTS > Le destinataire ne souhaite pas recevoir le fichier", conversation, &line, top_win, bottom_win);
									writeInConv("        Il a peut-être peur de vous", conversation, &line, top_win, bottom_win);
								}
								else
								{
									data.path = path;
									data.file_server_sock = file_server_sock;
									data.msg_server_sock = msg_server_sock;
									data.thread_status = &thread_status;
									data.mutex_thread_status = &mutex_thread_status;
									data.conversation = conversation;
									data.line = &line;
									data.top_win = top_win;
									data.bottom_win = bottom_win;

									thread_status = 1;
									if (pthread_create(&file_transfer, NULL, transferSendControl, &data) != 0)
									{
										writeInConv("< FTS > Le démarrage de l'envoi a échoué", conversation, &line, top_win, bottom_win);
										thread_status = 0;
									}
								}
							}
						}
						pthread_mutex_unlock(&mutex_thread_status);
					}
				}

				//verifie que le message n'est pas null
				else if(strcmp(msg_buffer, "") != 0)
				{
					sendServer(msg_server_sock, msg_buffer, strlen(msg_buffer)+1);
					
					//si le message n'est pas la commande "/list" on affiche le message
					if(strcmp(msg_buffer, "/list") != 0)
					{
						//ecrit le message dans la conversation en rajouter "vous" devant
						sprintf(buffer, "Vous : %s", msg_buffer);
			
						writeInConv(buffer, conversation, &line, top_win, bottom_win);
	        		}
	        	}
	        	//efface le buffer en entier
	        	for (i = 0; i < (int)strlen(msg_buffer); i++)
	        	{
	        		msg_buffer[i] = ' ';
	        	}
	        	msg_buffer[0] = '\0';
				werase(bottom_win);
				convRefresh(top_win, bottom_win);
				move(LINES - 2, 4); //on se remet au debut de la ligne du message
	        }
		}
	}
	
	clearMemory(msg_server_sock, file_server_sock, conversation, top_win, bottom_win);
	return EXIT_SUCCESS;
}
