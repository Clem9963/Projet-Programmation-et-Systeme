/* ************************************************************************* */
/* Exemple de client TCP/IP(v4). jean.connier@uca.fr                         */
/* ************************************************************************* */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

char *demandeIP();
char *demandePseudo();

int main( int argc, char * argv[] )
{
    int                socket_connexion;
    struct sockaddr_in adresse_serveur;
    char               buffer_adresse[128];
    
    char 			   texte[1000] = {0};
    ssize_t            taille_envoyee;
    ssize_t            taille_recue;
    char               buffer_message[1000];

	char * 	pseudo = demandePseudo();
    char *               adresseIP = demandeIP();
    
    /* Creation de la socket; AF_INET -> IPv4; SOCK_STREAM -> TCP           */
    socket_connexion = socket( AF_INET, SOCK_STREAM, 0 );
    if ( socket_connexion == -1 )
    {
        perror( "socket()" );
        exit( -1 );
    }
    /* On initialise la structure qui contient l'adresse ou se connecte.    */
    /* La fonction memset() remplit une zone avec ce qu'on veut (ici, 0).   */
    memset( &adresse_serveur, 0, sizeof( adresse_serveur ) );
    adresse_serveur.sin_family = AF_INET; /* IPv4 */

    /* On prend un port TCP au hasard : j'ai choisi 54878.                  *
     * Attention, il faut envoyer les 2 octets dans le bon ordre -> htons() */
    adresse_serveur.sin_port   = htons( 54878 );

    /* Le serveur ecoute sur l'IP interne de la machine (loopback);         *
     * inet_pton() transforme une adresse lisible (un string) en adresse    *
     * lisible par le systeme (une bouillie d'octets, dans le bon ordre)    */
    inet_pton( AF_INET, adresseIP, &(adresse_serveur.sin_addr.s_addr) );

    if (   connect(  socket_connexion
                   , (const struct sockaddr *) &adresse_serveur
                   , sizeof( adresse_serveur ) )
        == -1 )
    {
        perror( "connect()" );
        exit( -1 );
    }
    else
    {

	    printf(  "\nConnexion au serveur %s : OK\n"
	           , inet_ntop(  AF_INET                     /* Pour transformer la */
	                       , &(adresse_serveur.sin_addr) /* bouillie d'octets   */
	                       , buffer_adresse              /* en quelque chose    */
	                       , 128 ));


                /* Au pire on affichera n'importe quoi, sans deborder du buffer.    */
        buffer_message[999] = '\0'; 

        printf(  "%s\n", buffer_message );
				
        printf("Votre message : ");
    	scanf("%s", texte);
	    
	    /* On envoie notre message. Ici c'est le premier argument passe au      *
	     * programme. C'etait pas si complique, si ?                            */
	    taille_envoyee = send(  socket_connexion
	                          , strcat(strcat(pseudo, " : "), texte)
	                          , strlen(texte) + strlen(pseudo) + 3
	                          , 0 );

	    if ( taille_envoyee == -1 )
	    {
	        perror( "send()" );
	        exit( -1 );
	    }

	   	memset( buffer_message, 0, sizeof( buffer_message ) );
	    taille_recue = recv( socket_connexion, buffer_message, 1000, 0 );
        
        if ( taille_recue == -1 )
        {
            perror( "recv()" );
            exit( -1 );
        }
	}

    /* A ne pas oublier (moi j'avais oublie, ca m'a embete)                 */
    close( socket_connexion );

    return 0;
}

char *demandeIP(){
	char *adresseIP = malloc(sizeof(char) * 15);

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
	char *pseudo = malloc(sizeof(char) * 25);

	printf("\nVotre pseudo : ");
    scanf("%s",pseudo);
    return pseudo;
}