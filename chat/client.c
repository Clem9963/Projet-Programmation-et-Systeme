#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ncurses.h>

#define TAILLE_MESSAGE 1000

char *demandeIP();
char *demandePseudo();

struct message{
	char *pseudo;
	char *texte;
};

void concatener(char *texte, char * pseudo)
{
	char *temp = malloc(sizeof(char) * strlen(texte));
	strcpy(temp, "");
	strcat(temp, pseudo);
	strcat(temp, " : ");
	strcat(temp, texte);
	strcpy(texte, temp);
}

int main( int argc, char * argv[] )
{
	WINDOW *fenHaut, *fenBas;
	struct message msg;
	int i, j, param = 0, ligne = 0;

 
	char arreter[4] = "stop";

	msg.pseudo = demandePseudo();
    char * adresseIP = demandeIP(); //************se connecte meme quand adresse est fausse

    int socket_connexion;
    struct sockaddr_in adresse_serveur;
    char buffer_adresse[128];
    
    msg.texte = malloc(sizeof(char) * TAILLE_MESSAGE);
    ssize_t taille_envoyee;
    ssize_t taille_recue;
    char buffer_message[TAILLE_MESSAGE];

    
    // Creation de la socket; AF_INET -> IPv4; SOCK_STREAM -> TCP
    socket_connexion = socket( AF_INET, SOCK_STREAM, 0 );
    if ( socket_connexion == -1 )
    {
        perror( "socket()" );
        exit( -1 );
    }

    ///On initialise la structure qui contient l'adresse ou se connecte.
    //La fonction memset() remplit une zone avec ce qu'on veut (ici, 0).
    memset( &adresse_serveur, 0, sizeof( adresse_serveur ) );
    adresse_serveur.sin_family = AF_INET; // IPv4

    // On prend un port TCP au hasard : j'ai choisi 54878.
    // Attention, il faut envoyer les 2 octets dans le bon ordre -> htons()
    adresse_serveur.sin_port   = htons( 54878 );

    // Le serveur ecoute sur l'IP interne de la machine (loopback)
    // inet_pton() transforme une adresse lisible (un string) en adresse
    // lisible par le systeme (une bouillie d'octets, dans le bon ordre)
    inet_pton( AF_INET, adresseIP, &(adresse_serveur.sin_addr.s_addr) );

    //si on arrive pas a se connecter
    if (   connect(  socket_connexion
                   , (const struct sockaddr *) &adresse_serveur
                   , sizeof( adresse_serveur ) )
        == -1 )
    {
        perror( "connect()" );
        exit( -1 );
    }
    //si on arrive a se connecter
    else
    {

	    printf(  "\nConnexion au serveur %s : OK\n"
	           , inet_ntop(  AF_INET                     // Pour transformer la
	                       , &(adresse_serveur.sin_addr) // bouillie d'octets
	                       , buffer_adresse              // en quelque chose
	                       , 128 ));

	    //affiche la fenetre avec :  en haut lec hat et en bas la ligne pour ecrire des messages
	    initscr();
	    fenHaut = subwin(stdscr, LINES - 3, COLS, 0, 0);
	    fenBas = subwin(stdscr, 3, COLS, LINES - 3, 0);
	    box(fenHaut, ACS_VLINE, ACS_HLINE);
	    box(fenBas, ACS_VLINE, ACS_HLINE);
	    mvwprintw(fenHaut,1,1,"Messages : ");
	    mvwprintw(fenBas, 1, 1, "Votre message : ");
	    char **conversation = malloc(sizeof(char *) * (LINES - 6));	
	    for (i = 0; i < LINES - 6; i++)
	        conversation[i] = malloc(sizeof(char)* TAILLE_MESSAGE);

        //on peut ecrire des messages jusqu'a ce que le message est stop
	    while(1)
	    {
        	move(LINES - 2, 17);
        	getstr(msg.texte);
        	
        	//verifie si le message est egal a "stop" si oui on sort du chat
        	if(strlen(msg.texte) == 4)
        	{
        		param = 0;
        		for(i = 0; i < strlen(msg.texte); i++)
        		{
        			if(arreter[i] == msg.texte[i])
        				param++;
        		}
        	}
        	if(param == 4)
        		break;

	        //verif du message en fonction de sa longueur et de la longueur du pseudo
	        if(strlen(msg.texte) + strlen(msg.pseudo) + 3 < COLS - 2) // todo a changer
	        {
	        	//on assemble le pseudo et le message
	        	concatener(msg.texte, msg.pseudo);

		        // On envoie le message  
			    taille_envoyee = send(  socket_connexion
			                          , msg.texte
			                          , TAILLE_MESSAGE
			                          , 0 );

			    if ( taille_envoyee == -1 )
			    {
			        perror( "send()" );
			        exit( -1 );
			    }

			    //on ecrit le nouveau message dans le chat
			    if(ligne < LINES - 6)
			    {
			        strcpy(conversation[ligne], msg.texte);
			    	mvwprintw(fenHaut,ligne + 2, 1,conversation[ligne]);
			    	ligne++;
			    }
			    //sinon on scroll de un vers le bas
			    else
			    {
			    	//effacer l'affichage de la conversation
			    	for(i = 0; i < LINES - 6; i++)
			    		for(j = 0; j < strlen(conversation[i]); j++)
			    			mvwprintw(fenHaut, 2+i , 1+j," ");

			    	//decalage vers le haut
			    	for(i = 0; i < LINES - 7; i++)
			    	{
			    		strcpy(conversation[i], conversation[i+1]);
			    		mvwprintw(fenHaut, 2+i , 1,conversation[i]);
					}

					//ecriture du nouveau message en bas
			    	strcpy(conversation[LINES - 7], msg.texte);
			    	mvwprintw(fenHaut, 2 + LINES - 7, 1,conversation[LINES - 7]);
			    }

			    //supprimer le message "message trop long" si jamais il y etait avant
			    box(fenBas, ACS_VLINE, ACS_HLINE);
	        }
	        else
	        {
	        	mvwprintw(fenBas, 0, 1, "message trop long");
	        }

	        //supprimer le message qui a ete mis
	        for (i = 0; i < strlen(msg.texte); i++)
	        	msg.texte[i] = ' ';

	    	mvwprintw(fenBas, 1, 1, "Votre message : %s", msg.texte); 
	        wrefresh(fenHaut);
		    wrefresh(fenBas);
	    }

	    endwin();
	}

    // A ne pas oublier (moi j'avais oublie, ca m'a embete)
    close( socket_connexion );
    //free(fenHaut);//***************erreur dans les free
    //free(fenBas);

    return 0;
}

char *demandeIP(){
	char *adresseIP = malloc(sizeof(char) * 16);

	printf("\nAdresse IP du serveur : ");
    scanf("%s",adresseIP);
    
    if(strlen(adresseIP) > 15 || strlen(adresseIP) < 7)
    {
        fprintf( stderr, "Mauvaise adresse IP...\n" );
		exit(-1);
    }

    return adresseIP;
}

char *demandePseudo(){
	char *pseudo = malloc(sizeof(char) * 16);

	printf("\nVotre pseudo : ");
    scanf("%s",pseudo);

    while(strlen(pseudo) > 15)
    {
    	printf("\nVotre pseudo est trop long (au max 15 caracteres)");
    	printf("\nVotre pseudo : ");
    	scanf("%s",pseudo);
    }

    return pseudo;
}