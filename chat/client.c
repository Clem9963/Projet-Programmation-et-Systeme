#include "client.h"

int main(int argc, char *argv[]){

	//initialisation des variables
	char buffer[TAILLE_BUF];
	int sock;
	int ligne = 0, lettre, longueurMessageEntre = 0;
	char pseudo[TAILLE_PSEUDO];
	char ipAdresse[15]; 
	char bufferMessageEntre[TAILLE_BUF];
	fd_set readfds;	
	WINDOW *fenHaut, *fenBas;
	
	//récuperation du pseudo et de l'adresse ip
	demandePseudo(pseudo);
	demandeIP(ipAdresse);

	sock = connect_socket(ipAdresse, PORT);	//connexion au serveur
	write_serveur(sock, pseudo); //envoi le pseudo au serveur

	//Initialisation de l'interface graphique
	initscr();
	keypad(stdscr, TRUE); //pour récupérer les touches spéciales du clavier
	fenHaut = subwin(stdscr, LINES - 3, COLS, 0, 0);
    fenBas = subwin(stdscr, 3, COLS, LINES - 3, 0);
	initInterface(fenHaut, fenBas);

    //creation de la conversation (je suis obligé de faire l'initialisation de la conversation à cet endroit 
    //car il faut la faire après initscr())
	char conversation[LINES - 6][TAILLE_BUF];

	//se positionn au bon endroit pour ecrire le message
	move(LINES - 2, 11);

	//boucle du chat
	while(1)
	{
		FD_ZERO(&readfds); //met à 0 le readfds
		FD_SET(STDIN_FILENO, &readfds); //remplit le readfds
		FD_SET(sock, &readfds);

		if(select(sock + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			effaceMemoire(sock, fenHaut, fenBas);
			perror("select()");
			exit(-1);
		}

		//ecoute si il y a une activité sur le socket ou l'entrée standard
		if(FD_ISSET(sock, &readfds))
		{
			read_serveur(sock, buffer, conversation, &ligne, fenHaut, fenBas);
		}
		else if(FD_ISSET(STDIN_FILENO, &readfds))
		{
			//récupère la lettre entrée
			lettre = getch();
			
			//si ce n'est pas la touch entrée et que l'on n'a pas remplit la ligne
			//10 = touche entrée
			if(lettre != 10 && longueurMessageEntre < COLS - 14)
			{
				//on met la lettre dans le bufferMessagEntre
				bufferMessageEntre[longueurMessageEntre] = lettre;
				longueurMessageEntre++;
				move(LINES - 2, 11 + longueurMessageEntre); //met le curseur au bon endroit
				wrefresh(fenBas); //rafraichit
			}
			else
			{
				//si on est au bout de la ligne d'écriture on stop la saisie
				if(longueurMessageEntre >= COLS - 13-1)
					getnstr(" ", 0);
				
				//on met le buffer
				bufferMessageEntre[longueurMessageEntre] = '\0';
				longueurMessageEntre = 0;

				//se déconnecte du chat si le message est "/quit"
				if(strcmp(bufferMessageEntre, "/quit") == 0)
				{
					effaceMemoire(sock, fenHaut, fenBas);
					printf("\nDeconnexion réussie\n\n");
					exit(-1);
				}
				//verifie que le message n'est pas null
				else if(strcmp(bufferMessageEntre, "") != 0)
				{
					write_serveur(sock, bufferMessageEntre); //envoi le message au serveur
					
					//si le message n'est pas la commande "/list" on affiche le message
					if(strcmp(bufferMessageEntre, "/list") != 0)
					{
						//ecrit le message dans la conversation en rajouter "vous" devant
						concatener(bufferMessageEntre, "Vous");
						ecritDansConv(bufferMessageEntre, conversation, &ligne, fenHaut, fenBas);
	        		}
	        	}
	        	strcpy(bufferMessageEntre, " "); // on efface le buffer
	        	move(LINES - 2, 11); //on se remet au debut de la ligne du message
				wclrtoeol(fenBas); //on supprime le message saisie
				rafraichit(fenHaut, fenBas);
	        }
        }
    }

    //efface la memoire utilisée
	effaceMemoire(sock, fenHaut, fenBas);
	return 0;
}

//initialisation de l'interface
void initInterface(WINDOW *fenHaut, WINDOW *fenBas){
    box(fenHaut, ACS_VLINE, ACS_HLINE);
    box(fenBas, ACS_VLINE, ACS_HLINE);
    mvwprintw(fenHaut, 1, 1,"Messages : ");
    mvwprintw(fenBas, 1, 1, "Message : ");
}

int connect_socket(char *adresse, int port){
	int sock = socket(AF_INET, SOCK_STREAM, 0); //création d'un socket

	if(sock == -1)
	{
		perror("socket()");
		exit(-1);
	}

	struct hostent* hostinfo = gethostbyname(adresse); // information du serveur
	if(hostinfo == NULL)
	{
		perror("gethostbyname()");
		close(sock);
		exit(-1);
	}
	
	struct sockaddr_in sin; //structure qui contient les infos pour le socket
	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; //pour spécifier l'adresse
	sin.sin_port = htons(port); //pour le port 
	sin.sin_family = AF_INET; //pour le potocole
	
	//connexion au serveur
	if(connect(sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == -1)
	{
		perror("connect()");
		exit(-1);
	}

	return sock;
}

void read_serveur(int sock, char *buffer, char conversation[LINES - 6][TAILLE_BUF], int *ligne, WINDOW *fenHaut, WINDOW *fenBas){
	int taille_recue;

	taille_recue = recv(sock, buffer, TAILLE_BUF, 0); //reçoit le message

	//gere si il y a une erreur
	if(taille_recue == -1)
	{
		effaceMemoire(sock, fenHaut, fenBas);
		perror("recv()");
		exit(-1);
	}
	//pour gerer la deconnexion du serveur
	else if(taille_recue == 0)
	{
		effaceMemoire(sock, fenHaut, fenBas);
		printf("\nla connexion au serveur a été interrompue\n\n");
		exit(-1);
	}
	else
	{
		//verifie le message et met "..." a la fin si le message est trop lonng
		if(strlen(buffer) > COLS - 2 )
		{
			buffer[COLS - 5] = '.';
			buffer[COLS - 4] = '.';
			buffer[COLS - 3] = '.';
			buffer[COLS - 2] = '\0';
		}

		//ecrit le message dans la conversation
		buffer[taille_recue] = '\0';
		ecritDansConv(buffer, conversation, ligne, fenHaut, fenBas);
	}	
}

//envoi message au serveur
void write_serveur(int sock, char *buffer){
	if(send(sock, buffer, strlen(buffer), 0) == -1)
	{
		endwin();
		perror("send()");
		close(sock);
		exit(-1);
	}
}

//demande uen adresse ip
void demandeIP(char *adresseIP){
	printf("\nAdresse IP du serveur : ");
    scanf("%s", adresseIP);
    
    if(strlen(adresseIP) > 15 || strlen(adresseIP) < 7)
    {
        fprintf(stderr, "Mauvaise adresse IP...\n");
		exit(-1);
    }
}

//demande un pseudo
void demandePseudo(char *pseudo){
	printf("\nVotre pseudo : ");
    scanf("%s", pseudo);

    while(strlen(pseudo) > TAILLE_PSEUDO)
    {
    	printf("\nVotre pseudo est trop long (au max 15 caracteres)");
    	printf("\nVotre pseudo : ");
    	scanf("%s", pseudo);
    }
}

//ecrit un message dans la conversation
void ecritDansConv(char *buffer, char conversation[LINES - 6][TAILLE_BUF], int *ligne, WINDOW *fenHaut, WINDOW *fenBas){
	int i, j;

	//on ecrit le nouveau message dans le chat
    if(*ligne < LINES - 6)
    {
        strcpy(conversation[*ligne], buffer);
    	mvwprintw(fenHaut, *ligne + 2, 1, conversation[*ligne]);
    	(*ligne)++;
    }
    //sinon on scroll de un vers le bas
    else
    {
    	//effacer l'affichage de la conversation
		werase(fenHaut);
		mvwprintw(fenHaut, 1, 1,"Messages : "); //remet le texte "messages :"

    	//decalage vers le haut
    	for(i = 0; i < LINES - 7; i++)
    	{
    		strcpy(conversation[i], conversation[i + 1]);
    		mvwprintw(fenHaut, 2 + i , 1,conversation[i]);
		}

		//ecriture du nouveau message en bas
    	strcpy(conversation[LINES - 7], buffer);
    	mvwprintw(fenHaut, 2 + LINES - 7, 1, conversation[LINES - 7]);
    }
	
	//raffraichit les fenetres
	rafraichit(fenHaut, fenBas);

}

//raffraichit l'interface
void rafraichit(WINDOW *fenHaut, WINDOW *fenBas){
    box(fenBas, ACS_VLINE, ACS_HLINE);//recrée les cadres
    box(fenHaut, ACS_VLINE, ACS_HLINE);
    wrefresh(fenHaut);
    wrefresh(fenBas);
}

//concatene le pseudo dans le buffer pour faire "pseudo : message" (+ propre)
void concatener(char *buffer, char *pseudo){
	char temp[TAILLE_BUF + TAILLE_PSEUDO];
	sprintf(temp, "%s : %s", pseudo, buffer);
	strcpy(buffer, temp);
}

//supprime les fenetres et l'interface et ferme le socket
void effaceMemoire(int sock, WINDOW *fenHaut, WINDOW *fenBas){
	delwin(fenHaut);
	delwin(fenBas);
	endwin();
	close(sock);
}