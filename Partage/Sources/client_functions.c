#include <errno.h>
#include <ncurses.h>
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

int verifyDirectory(char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
{
	/* Cette fonction vérifie si le chemin "path" est valide */
	FILE *f = NULL;

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
	/* Cette fonction prend en paramètre une requête brute, un buffer pour intérroger le client et le path final de réception */
	/* Elle s'occupe de demander au client destinataire s'il souhaite recevoir le fichier */

	const char* home_directory = getenv("HOME");
	char buffer[4];
	char msg_buffer[BUFFER_SIZE] = "";	// Buffer pour les messages d'erreurs

	char *file_name = NULL;
	FILE *f = NULL;
	int answer = -1;
	
	strcpy(path, home_directory);
	strcat(path, "/File_Transfer/");
	if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
	{
		if (errno != EEXIST)			// errno vaut EEXIST si et seulement si le dossier existe déjà
		{
			endwin();
			perror("mkdir error");
			return 0;
		}
	} 
	file_name = strrchr(request, '/')+1;
	strcat(path, file_name);

	f = fopen(path, "r");
	if (f == NULL)
	{
		sprintf(msg_buffer, "< FTS > Voulez-vous recevoir le fichier %s ?", file_name);
		writeInConv(msg_buffer, conversation, line, top_win, bottom_win);
		writeInConv("1 : Oui, 0 : Non", conversation, line, top_win, bottom_win);
		while (answer != 0 && answer != 1)
		{
			werase(bottom_win);					// On efface le message entamé pour attendre la réponse d'acceptation de la réception
			convRefresh(top_win, bottom_win);	// Rafraichissement de l'interface
			getnstr(buffer, 1);					// Récupération de la saisie

			answer = atoi(buffer);
			
			if (answer != 0 && answer != 1)
			{
				writeInConv("< FTS > Vous n'avez pas saisi un nombre valide !", conversation, line, top_win, bottom_win);
			}
		}

		return answer;
	}
	else
	{
		sprintf(msg_buffer, "< FTS > Quelqu'un souhaite vous envoyer le fichier %s mais il existe déjà", file_name);
		writeInConv(msg_buffer, conversation, line, top_win, bottom_win);
		sprintf(msg_buffer, "(%s)", path);
		writeInConv(msg_buffer, conversation, line, top_win, bottom_win);
		fclose(f);
		return 0;
	}
}

int verifySendingRequest(char *buffer, char *dest_username, char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
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
	/* Fonction gérant l'envoi du fichier */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";
	long int size = 0;
	int residue_size = 0;
	int package_number = 0;
	int i = 0;

	pthread_mutex_lock(data->mutex_thread_status);		// Cette fonction est bloquante jusqu'à ce que le mutex puisse être vérouillé

	FILE *f = NULL;

	f = fopen(data->path, "rb");
	if (f == NULL)
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);

		fclose(f);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		writeInConv("< FTS > Envoi impossible : le chemin n'est pas valide ou le fichier est inexistant", data->conversation, data->line, data->top_win, data->bottom_win);
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
			writeInConv("< FTS > Le transfert en est à 25%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 50%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 75%", data->conversation, data->line, data->top_win, data->bottom_win);
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
	writeInConv("< FTS > L'envoi s'est parfaitement déroulé !", data->conversation, data->line, data->top_win, data->bottom_win);
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
	sprintf(buffer, "< FTS > Réception du fichier dans %s\n", data->path);
	writeInConv(buffer, data->conversation, data->line, data->top_win, data->bottom_win);
	f = fopen(data->path, "wb");
	if (f == NULL)
	{
		strcpy(buffer, "/abort");
		sendServer(data->msg_server_sock, buffer, strlen(buffer)+1);

		fclose(f);
		*(data->thread_status) = -1;
		pthread_mutex_unlock(data->mutex_thread_status);
		writeInConv("< FTS > Réception impossible : le chemin n'est pas valide", data->conversation, data->line, data->top_win, data->bottom_win);
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
			writeInConv("< FTS > Le transfert en est à 25%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 50%", data->conversation, data->line, data->top_win, data->bottom_win);
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			writeInConv("< FTS > Le transfert en est à 75%", data->conversation, data->line, data->top_win, data->bottom_win);
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
	writeInConv("< FTS > La réception s'est parfaitement déroulée !", data->conversation, data->line, data->top_win, data->bottom_win);
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


void initInterface(WINDOW *top_win, WINDOW *bottom_win)
{
	box(top_win, ACS_VLINE, ACS_HLINE);
	box(bottom_win, ACS_VLINE, ACS_HLINE);
	mvwprintw(top_win, 1, 1,"Messages : ");
	mvwprintw(bottom_win, 1, 1, "Message : ");
}

void writeInConv(char *buffer, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win)
{
	int i = 0;

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
	/* Rafraîchit l'interface */
	box(bottom_win, ACS_VLINE, ACS_HLINE);	//recrée les cadres
	box(top_win, ACS_VLINE, ACS_HLINE);
	mvwprintw(bottom_win, 1, 1, "Message : ");
	wrefresh(top_win);
	wrefresh(bottom_win);
}

int recvServer(int sock, char *buffer, size_t buffer_size)
{
	ssize_t recv_outcome = 0;
	recv_outcome = recv(sock, buffer, buffer_size, 0);
	if (recv_outcome == SOCKET_ERROR)
	{
		endwin();
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
