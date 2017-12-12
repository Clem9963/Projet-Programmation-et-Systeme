#include "client.h"

int main(int argc, char *argv[]){

	//initialisation des variables
	char buffer[TAILLE_BUF];
	int sock;
	int ligne = 0;
	char *pseudo = demandePseudo();
	char *ipAdresse = demandeIP();
	char **conversation;
	fd_set readfds;	
	WINDOW *fenHaut, *fenBas;
	
	sock = connect_socket(ipAdresse, PORT);	//connexion au serveur
	write_serveur(sock, pseudo); //envoi le pseudo au serveur
	
	//libere la memoire du pseudo et adresse ip car plus besoin
	free(pseudo);
	free(ipAdresse);

	//Initialisation de l'interface graphique
	initscr();
	fenHaut = subwin(stdscr, LINES - 3, COLS, 0, 0);
    fenBas = subwin(stdscr, 3, COLS, LINES - 3, 0);
	initInterface(fenHaut, fenBas);

    //creation de la conversation
    conversation = initConv();

	//boucle du chat
	while(1)
	{
		move(LINES - 2, 11);//se positionn au bon endroit pour ecrire le message
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(sock, &readfds);

		if(select(sock + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			endwin();
			perror("select()");
			exit(-1);
		}

		if(FD_ISSET(sock, &readfds))
		{
			read_serveur(sock, buffer, conversation, &ligne, fenHaut, fenBas);
		}
		else if(FD_ISSET(STDIN_FILENO, &readfds))
		{
			getnstr(buffer, COLS - 13); //bloque si la ligne est remplie
			buffer[strlen(buffer)] = '\0';
			
			//se déconnecte du chat si le message est deconnexion
			if(strcmp(buffer, "deconnexion") == 0)
			{
				endwin();
				printf("\nDeconnexion réussie\n\n");
				libereMemoire(sock, conversation);
				exit(-1);
			}
			else
			{
				write_serveur(sock, buffer); //envoi le message au serveur
				
				//ecrit le message dans la conversation en rajouter "vous" devant
				concatener(buffer, "Vous");
				ecritDansConv(buffer, conversation, &ligne, fenHaut, fenBas);
        	}
        }
    }

    //liberation de la memoire
    libereMemoire(sock, conversation);
	return 0;
}

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

//initialise la conversation
char **initConv(){
	int i;
	char **conv = malloc(sizeof(char *) * (LINES - 6));

    for (i = 0; i < LINES - 6; i++)
        conv[i] = malloc(sizeof(char) * TAILLE_BUF);

    return conv;
}

void read_serveur(int sock, char *buffer, char **conversation, int *ligne, WINDOW *fenHaut, WINDOW *fenBas){
	int taille_recue;

	taille_recue = recv(sock, buffer, TAILLE_BUF, 0);

	if(taille_recue == -1)
	{
		endwin();
		perror("recv()");
		exit(-1);
	}
	//pour gerer la deconnexion du serveur
	else if(taille_recue == 0)
	{
		endwin();
		printf("\nla connexion au serveur a été interrompue\n\n");
		//liberation de la memoire
		libereMemoire(sock, conversation);
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
		exit(-1);
	}
}

//demande uen adresse ip
char *demandeIP(){
	char *adresseIP = malloc(sizeof(char) * 15);

	printf("\nAdresse IP du serveur : ");
    scanf("%s", adresseIP);
    
    if(strlen(adresseIP) > 15 || strlen(adresseIP) < 7)
    {
        fprintf(stderr, "Mauvaise adresse IP...\n");
		exit(-1);
    }

    return adresseIP;
}

//demande un pseudo
char *demandePseudo(){
	char *pseudo = malloc(sizeof(char) * TAILLE_PSEUDO);

	printf("\nVotre pseudo : ");
    scanf("%s", pseudo);

    while(strlen(pseudo) > TAILLE_PSEUDO)
    {
    	printf("\nVotre pseudo est trop long (au max 15 caracteres)");
    	printf("\nVotre pseudo : ");
    	scanf("%s", pseudo);
    }

    return pseudo;
}

//ecrit un message dans la conversation
void ecritDansConv(char *buffer, char **conversation, int *ligne, WINDOW *fenHaut, WINDOW *fenBas){
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
    	for(i = 0; i < LINES - 6; i++)
    		for(j = 0; j < strlen(conversation[i]); j++)
    			mvwprintw(fenHaut, 2 + i , 1 + j, " ");

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

void rafraichit(WINDOW *fenHaut, WINDOW *fenBas){
	wclrtoeol(fenBas);
    box(fenBas, ACS_VLINE, ACS_HLINE);
    box(fenHaut, ACS_VLINE, ACS_HLINE);
    wrefresh(fenHaut);
    wrefresh(fenBas);
}

//concatene le pseudo dans le buffer pour faire "pseudo : message" (+ propre)
void concatener(char *buffer, char *pseudo){
	char *temp = malloc(sizeof(char) * (TAILLE_BUF + TAILLE_PSEUDO));
	sprintf(temp, "%s : %s", pseudo, buffer);
	strcpy(buffer, temp);
	free(temp);
}

void libereMemoire(int sock, char **conversation){
	int i;

    //liberation de la memoire
    for (i = 0; i < LINES - 6; i++)
    	free(conversation[i]);
   	free(conversation);
	close(sock);
}