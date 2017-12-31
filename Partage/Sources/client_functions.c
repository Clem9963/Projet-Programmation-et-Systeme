#include "client.h"
#include "client_functions.h"

int verifyDirectory(char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
{
	/* Cette fonction vérifie si le chemin "path" est valide. A noter qu'elle affiche un message
	sur l'interface graphique si le chemin est erroné.
	Elle prend cinq arguments : un pointeur sur char représentant le chemin du fichier à vérifier,
	un double pointeur sur char correspondant à la conversation en cours, un pointeur sur int représentant
	la ligne en cours et deux pointeurs sur WINDOW associés respectivement au cadre supérieur et au cadre
	inférieur de l'interface graphique.
	La fonction retourne un booléen qui vaut vrai seulement si le chemin existe. */

	FILE *f = NULL;			// Flux de test

	f = fopen(path, "r");
	if (f == NULL)
	{
		writeInConv("< FTS > Le chemin n'est pas valide ou le fichier est inexistant", conversation, line, top_win, bottom_win);
		return FALSE;
	}
	else
	{
		fclose(f);
		return TRUE;
	}
}

int answerSendingRequest(char *request, char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
{
	/* Cette fonction s'occupe de demander au client destinataire s'il souhaite recevoir le fichier.
	Elle prend en arguments, une requête brute, le path final de réception qui sera modifié par la fonction,
	un double pointeur sur char correspondant à la conversation en cours, un pointeur sur int représentant
	la ligne en cours et deux pointeurs sur WINDOW associés respectivement au cadre supérieur et au cadre
	inférieur de l'interface graphique.
	La fonction retourne un int représentant la réponse du destinataire. Cet entier vaut 1 en cas d'acceptation, 0 sinon */

	const char* home_directory = getenv("HOME");
	char buffer[4];							// Buffer detiné à la récupération de la réponse du client
	char msg_buffer[BUFFER_SIZE] = "";		// Buffer pour les messages d'erreurs

	char *file_name = NULL;					// Pointeur qui servira à repérer le nom du fichier dans la requête
	FILE *f = NULL;							// Flux de test
	int answer = -1;						// Initialisation de answer (valeur non attendue pour rentrer dans la boucle)
	
	strcpy(path, home_directory);
	strcat(path, "/File_Transfer/");
	if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
	{
		/* On tente de créer le répertoire de destination en accordant les droits
		de lecture, d'écriture et d'éxecution seulement à l'utilisateur. */
		if (errno != EEXIST)			// errno vaut EEXIST si et seulement si le dossier existe déjà
		{
			endwin();
			perror("mkdir error");
			exit(EXIT_FAILURE);
		}
	} 
	file_name = strrchr(request, '/')+1;
	strcat(path, file_name);			// path représente à présent le chemin de réception du fichier

	f = fopen(path, "r");
	if (f == NULL)						// path vaut NULL seulement si le fichier n'existe pas déjà
	{
		sprintf(msg_buffer, "< FTS > Voulez-vous recevoir le fichier %s ?", file_name);
		writeInConv(msg_buffer, conversation, line, top_win, bottom_win);
		writeInConv("        1 : Oui, 0 : Non", conversation, line, top_win, bottom_win);
		while (answer != 0 && answer != 1)
		{
			werase(bottom_win);					// On efface le message entamé pour attendre la réponse d'acceptation de la réception
			convRefresh(top_win, bottom_win);	// Rafraichissement de l'interface
			move(LINES - 2, 4);
			getnstr(buffer, 1);					// Récupération de la saisie
			
			werase(bottom_win);					// On rééfface juste après avoir récupéré la saisie
			convRefresh(top_win, bottom_win);
			move(LINES - 2, 4);

			answer = atoi(buffer);				// Affectation de answer en conséquence
			
			if (answer != 0 && answer != 1)
			{
				writeInConv("< FTS > Vous n'avez pas saisi un nombre valide !", conversation, line, top_win, bottom_win);
			}
		}

		return answer;
	}
	else	// La fichier a pu être ouvert et donc existe déjà !
	{
		sprintf(msg_buffer, "< FTS > Quelqu'un souhaite vous envoyer le fichier %s mais il existe déjà", file_name);
		writeInConv(msg_buffer, conversation, line, top_win, bottom_win);
		sprintf(msg_buffer, "        (%s)", path);
		writeInConv(msg_buffer, conversation, line, top_win, bottom_win);
		fclose(f);	// On n'oublie pas de fermer le fichier
		return 0;
	}
}

int verifySendingRequest(char *buffer, char *dest_username, char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
{
	/* Fonction vérifiant l'intégrité et la validité d'une requête /sendto.
	Elle prend en arguments un pointeur sur char correspondant au buffer (c'est-à-dire à
	la requête brute), un pointeur sur char représentant le nom de l'utilisateur destinataire,
	un pointeur sur char associé au path d'émission, un double pointeur sur char correspondant à la conversation en cours,
	un pointeur sur int représentant la ligne en cours et deux pointeurs sur WINDOW associés respectivement au cadre
	supérieur et au cadre inférieur de l'interface graphique.
	Les tableaux dest_username et path sont remplis en fonction.
	La fonction retourne un booléen qui vaut vrai seulement si la requête et correcte et intègre. */

	int i = 0;
	char *char_ptr = NULL;			// Pointeur de recherche dans la requête

	char_ptr = strchr(buffer, ' ');	// On cherche le premier espace précédent l'username
	if (char_ptr != NULL)
	{
		i = 0;
		for (char_ptr += 1; char_ptr < buffer + strlen(buffer) && *char_ptr != ' '; char_ptr++)
		{
			if (i == USERNAME_SIZE)
			{
				writeInConv("< FTS > Le pseudonyme est trop long !", conversation, line, top_win, bottom_win);
				return FALSE;
			}
			dest_username[i] = *char_ptr;
			i++;
		}
		dest_username[i] = '\0';

		i = 0;
		for (char_ptr += 1; char_ptr < buffer + strlen(buffer); char_ptr++)
		{
			path[i] = *char_ptr;
			i++;
		}
		path[i] = '\0';
	}
	else
	{
		writeInConv("< FTS > Rappel de syntaxe : /sendto <dest_username> <path>", conversation, line, top_win, bottom_win);
		return FALSE;
	}

	if (path[0] == '\0')
	{
		writeInConv("< FTS > Rappel de syntaxe : /sendto <dest_username> <path>", conversation, line, top_win, bottom_win);
		return FALSE;
	}

	return TRUE;
}

void *transferSendControl(void *src_data)
{
	/* Fonction gérant l'envoi du fichier
	Elle prend en arguments un pointeur sur void qui sera casté par
	la suite en un pointeur sur une structure struct TransferDetails.
	La fonction retourne toujours NULL */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";						// Buffer servant à l'envoi des paquets
	long int size = 0;									// Taille en octets du fichiers à envoyer
	int residue_size = 0;								// Taille du dernier paquet (vaut 0 si size est un multiple de 1 024)
	int package_number = 0;								// Nombre de paquets de 1 024 octets
	int i = 0;

	pthread_mutex_lock(data->mutex_thread_status);		// Cette fonction est bloquante jusqu'à ce que le mutex puisse être vérouillé

	FILE *f = NULL;

	f = fopen(data->path, "rb");
	if (f == NULL)
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);	// Envoi d'une requête de type "/abort"

		fclose(f);														// On n'oublie pas de fermer le fichier
		*(data->thread_status) = -1;									// thread_status est affecté pour indiquer que le thread attend que l'on lise son code de retour
		pthread_mutex_unlock(data->mutex_thread_status);				// On n'oublie pas de déverouiller le mutex
		writeInConv("< FTS > Envoi impossible : le chemin n'est pas valide ou le fichier est inexistant", data->conversation, data->line, data->top_win, data->bottom_win);
		pthread_exit(NULL);
	}

	fseek(f, 0, SEEK_END);

	size = ftell(f);
	package_number = size / BUFFER_SIZE;
	residue_size = size % BUFFER_SIZE;
	sprintf(buffer, "%d %d", package_number, residue_size);			// '\0' écrit par sprintf
	sendServer(data->file_server_sock, buffer, strlen(buffer)+1);
	recvServer(data->file_server_sock, buffer, 1);		// Accusé de réception de 1 octet pour la synchronisation

	fseek(f, 0, SEEK_SET);

	for (i = 0; i < package_number; i++)	// Traitement des paquets normaux (de 1024 octets)
	{
		if (i == package_number / 4 && package_number >= 500000)	// On affiche le pourcentage seulement si le nombre de paquets est élevé
		{
			writeInConv("< FTS > Le transfert en est à 25%%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 50%%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 75%%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		
		fread(buffer, BUFFER_SIZE, 1, f);
		// Pas de '\0' pour fermer le buffer car il risque déjà d'y avoir des caractères NULL dans le buffer
		sendServer(data->file_server_sock, buffer, sizeof(buffer));

		/* Accusé de réception (pour la synchronisation) */
		recvServer(data->file_server_sock, buffer, 1);
	}
	if (residue_size != 0)					// Traitement du dernier paquet s'il n'est pas constitué de 1 024 octets
	{
		fread(buffer, residue_size, 1, f);
		sendServer(data->file_server_sock, buffer, residue_size);

		/* Accusé de réception (pour la synchronisation) */
		recvServer(data->file_server_sock, buffer, 1);
	}

	fclose(f);
	*(data->thread_status) = -1;						// thread_status est affecté pour indiquer que le thread attend que l'on lise son code de retour
	pthread_mutex_unlock(data->mutex_thread_status);	// On n'oublie pas de déverouiller le mutex
	writeInConv("< FTS > L'envoi s'est parfaitement déroulé !", data->conversation, data->line, data->top_win, data->bottom_win);
	pthread_exit(NULL);
}

void *transferRecvControl(void *src_data)
{
	/* Fonction gérant la réception du fichier
	Elle prend en arguments un pointeur sur void qui sera casté par
	la suite en un pointeur sur une structure struct TransferDetails.
	La fonction retourne toujours NULL */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";				// Buffer servant à l'envoi des paquets
	char *char_ptr = NULL;						// Pointeur sur char pour rechercher un caractère dans un tableau de char
	int package_number = 0;						// Nombre de paquets de 1 024 octets
	int residue_size = 0;						// Taille du dernier paquet
	int i = 0;

	pthread_mutex_lock(data->mutex_thread_status);		// Cette fonction est bloquante jusqu'à ce que le mutex puisse être vérouillé

	FILE *f = NULL;
	sprintf(buffer, "< FTS > Réception du fichier dans %s\n", data->path);
	writeInConv(buffer, data->conversation, data->line, data->top_win, data->bottom_win);
	f = fopen(data->path, "wb");
	if (f == NULL)		// S'il est impossible d'ouvrir le fichier
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);	// Envoi d'une requête de type "/abort"

		fclose(f);														// On n'oublie pas de fermer le fichier
		*(data->thread_status) = -1;									// thread_status est affecté pour indiquer que le thread attend que l'on lise son code de retour
		pthread_mutex_unlock(data->mutex_thread_status);				// On n'oublie pas de déverouiller le mutex
		writeInConv("< FTS > Réception impossible : le chemin n'est pas valide", data->conversation, data->line, data->top_win, data->bottom_win);
		pthread_exit(NULL);
	}

	recvServer(data->file_server_sock, buffer, sizeof(buffer));		// Réception du nombre de paquets et de la taille du dernier paquet
	char_ptr = strchr(buffer, ' ');
	*char_ptr = '\0';
	package_number = atoi(buffer);
	residue_size = atoi(char_ptr + 1);
	buffer[0] = -1;										// On met tous les bits à 1 pour le premier octet (génération de l'accusé de réception)
	sendServer(data->file_server_sock, buffer, 1);		// Accusé de réception pour la synchronisation

	for (i = 0; i < package_number; i++)
	{
		if (i == package_number / 4 && package_number >= 500000)	// On affiche le pourcentage seulement si le nombre de paquets est élevé
		{
			writeInConv("< FTS > Le transfert en est à 25%%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 50%%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 75%%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		
		recvServer(data->file_server_sock, buffer, sizeof(buffer));
		fwrite(buffer, sizeof(buffer), 1, f);

		buffer[0] = -1;									// On met tous les bits à 1 pour le premier octet (génération de l'accusé de réception)
		sendServer(data->file_server_sock, buffer, 1);	// Accusé de réception pour la synchronisation
	}
	if (residue_size != 0)
	{
		recvServer(data->file_server_sock, buffer, sizeof(buffer));
		fwrite(buffer, residue_size, 1, f);

		buffer[0] = -1;									// On met tous les bits à 1 pour le premier octet
		sendServer(data->file_server_sock, buffer, 1);	// Accusé de réception pour la synchronisation
	}

	fclose(f);
	*(data->thread_status) = -1;						// thread_status est affecté pour indiquer que le thread attend que l'on lise son code de retour
	pthread_mutex_unlock(data->mutex_thread_status);	// On n'oublie pas de déverouiller le mutex
	writeInConv("< FTS > La réception s'est parfaitement déroulée !", data->conversation, data->line, data->top_win, data->bottom_win);
	pthread_exit(NULL);
}

void connectSocket(char* address, int port, int *msg_server_sock, int *file_server_sock)
{
	/* Cette fonction permet de se connecter au serveur.
	Elle prend en paramètre un pointeur sur char représentant l'adresse, un entier correspondant au port du serveur
	et deux pointeurs sur int associés respectivement à la socket relative aux messages et à la socket relative au transfert
	de fichiers. */

	*msg_server_sock = socket(AF_INET, SOCK_STREAM, 0);
	*file_server_sock = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent* hostinfo;
	struct sockaddr_in sin; 	// structure qui possède toutes les informations pour la socket

	/* Connexion de la socket pour envoyer/recevoir des messages */

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

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr;	// on spécifie l'adresse 
	sin.sin_port = htons(port); 						// le port
	sin.sin_family = AF_INET;							// et le protocole (AF_INET pour IP)

	if (connect(*msg_server_sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR)	// Demande de connexion
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

	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr;	// on spécifie l'adresse 
	sin.sin_port = htons(port);							// le port
	sin.sin_family = AF_INET;							// et le protocole (AF_INET pour IP)

	if (connect(*file_server_sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == SOCKET_ERROR)	// Demande de connexion
	{
		perror("< FERROR > connect error");
		exit(errno);
	}
}


void initInterface(WINDOW *top_win, WINDOW *bottom_win)
{
	/* Cette fonction initialise l'interface graphique.
	Elle prend en arguments deux pointeurs sur WINDOW représentant respectivement
	le cadre du haut et le cadre du bas de l'interface graphique. */

	box(top_win, ACS_VLINE, ACS_HLINE);
	box(bottom_win, ACS_VLINE, ACS_HLINE);
	mvwprintw(top_win, 1, 1,"Messages : ");
	mvwprintw(bottom_win, 1, 1, " : ");
}

void writeInConv(char *buffer, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
{
	/* Cette fonction écrit un nouveau message dans la conversation
	Elle prend en argument le buffer contenant le message, un double pointeur sur char correspondant à
	la conversation en cours, un pointeur sur int représentant la ligne en cours et deux pointeurs sur
	WINDOW associés respectivement au cadre supérieur et au cadre inférieur de l'interface graphique.*/

	int i = 0;

	//verifie le message et met "..." a la fin si le message est trop lonng
	if((int)strlen(buffer) > COLS - 2 )
	{
		buffer[COLS - 5] = '.';
		buffer[COLS - 4] = '.';
		buffer[COLS - 3] = '.';
		buffer[COLS - 2] = '\0';
	}

	//on ecrit le nouveau message dans le chat
	if(*line < LINES - 6)
	{
		strcpy(conversation[*line], buffer);
		mvwprintw(top_win, *line + 2, 1, conversation[*line]);
		(*line)++;
	}
	//sinon on scroll de un vers le bas
	else
	{
		//effacer l'affichage de la conversation
		werase(top_win);
		mvwprintw(top_win, 1, 1,"Messages : "); //remet le texte "messages :"

		//decalage vers le haut
		for(i = 0; i < LINES - 7; i++)
		{
			strcpy(conversation[i], conversation[i + 1]);
			mvwprintw(top_win, 2 + i , 1,conversation[i]);
		}

		//ecriture du nouveau message en bas
		strcpy(conversation[LINES - 7], buffer);
		mvwprintw(top_win, 2 + LINES - 7, 1, conversation[LINES - 7]);
	}
	
	//raffraichit les fenetres
	convRefresh(top_win, bottom_win);
}

void convRefresh(WINDOW *top_win, WINDOW *bottom_win)
{
	/* Rafraîchit l'interface
	Cette fonction prend en arguments deux pointeurs sur WINDOW représentant respectivement
	le cadre du haut et le cadre du bas de l'interface graphique */

	box(bottom_win, ACS_VLINE, ACS_HLINE);	//recrée les cadres
	box(top_win, ACS_VLINE, ACS_HLINE);
	mvwprintw(bottom_win, 1, 1, " : ");
	wrefresh(top_win);
	wrefresh(bottom_win);
}

int recvServer(int sock, char *buffer, size_t buffer_size)
{
	/* Cette fonction reçoit un message ou un paquet du serveur et s'occupe des tests.
	Elle prend en paramètres la socket, un buffer dans lequel stocker le message
	et un entier représentant la taille du buffer en question.
	Elle retourne un booléen qui vaut FALSE seulement si le serveur n'est
	plus joignable. */

	ssize_t recv_outcome = 0;
	recv_outcome = recv(sock, buffer, buffer_size, 0);
	if (recv_outcome == SOCKET_ERROR)
	{
		endwin();				// Fermeture de l'interface graphique avant le perror
		perror("< FERROR > recv error");
		exit(errno);
	}
	else if (recv_outcome == 0)
	{
		return FALSE;
	}

	return TRUE;
}

void sendServer(int sock, char *buffer, size_t buffer_size)	// On pourra mettre strlen(buffer)+1 pour le dernier argument pour les chaînes de caractères
{
	/* Cette fonction envoie un message ou un paquet au serveur et s'occupe des tests.
	Elle prend en paramètres la socket du client en question, un buffer contenant le message
	et un entier représentant la taille du buffer en question. */

	if (send(sock, buffer, buffer_size, 0) == SOCKET_ERROR)
	{
		endwin();				// Fermeture de l'interface graphique avant le perror
		perror("< FERROR > send error");
		exit(errno);
	}
}

void clearMemory(int msg_server_sock, int file_server_sock, char **conversation, WINDOW *top_win, WINDOW *bottom_win)
{
	/* Cette fonction ferme les sockets et efface les cadres de l'interface et la conversation de la mémoire
	Elle prend en argument la socket relative au message, la socket relative aux fichiers, un double pointeur
	sur char associé à la conversation ainsi que deux pointeurs sur WINDOW  correspondant respectivement au
	cadre du haut et au cadre du bas de l'interface graphique. */

	int i = 0;

	close(msg_server_sock);
	close(file_server_sock);
	delwin(top_win);
	delwin(bottom_win);
	for (i = 0; i < LINES - 6; i++)
	{
		free(conversation[i]);
	}
	free(conversation);

}