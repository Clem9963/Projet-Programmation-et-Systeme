/* ************************************************************************* */
/* Exemple de serveur TCP/IP(v4). jean.connier@uca.fr                        */
/* ************************************************************************* */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TAILLE_MESSAGE 1000

int main( void )
{
    int                socket_passive;      /* Socket d'ecoute              */
    int                socket_client;       /* Celle pour traiter un client */
    struct sockaddr_in adresse_ecoute;      /* Adresse locale ou le serveur *
                                             * ecoutera                     */
    struct sockaddr_in adresse_client;      /* Adresse du client connecte   */
    char               buffer_adresse[128]; /* Buffer pour mettre un string *
                                             * contenant adresse du client  */
    socklen_t          taille;              /* Taille pour accept()         */
    ssize_t            taille_recue;        /* Quantite d'octets recue      */
    char *buffer_message = malloc(sizeof(char) * TAILLE_MESSAGE); /* Buffer pour mettre le        *
                                             * message recu et l'afficher   */
	ssize_t            taille_envoyee;

    /* Creation de la socket; AF_INET -> IPv4; SOCK_STREAM -> TCP           */
    socket_passive = socket( AF_INET, SOCK_STREAM, 0 );
    if ( socket_passive == -1 )
    {
        perror( "socket()" );
        exit( -1 );
    }

    /* On initialise la structure qui contient l'adresse ou on ecoute.      */
    /* La fonction memset() remplit une zone avec ce qu'on veut (ici, 0).   */
    memset( &adresse_ecoute, 0, sizeof( adresse_ecoute ) );
    adresse_ecoute.sin_family = AF_INET; /* IPv4 */

    /* Le serveur ecoute sur le port 54878. Choisi au hasard; c'est l'elu.  *
     * Attention, il faut envoyer les 2 octets dans le bon ordre -> htons() */
    adresse_ecoute.sin_port   = htons( 54878 );
    /* Petit bricolage pour l'adresse : je mets juste 0. Ce qui veut dire   *
     * 0x00000000, et ca, dans n'importe quel ordre, c'est l'IP 0.0.0.0.    *
     * 0.0.0.0 signifie "ecoute sur n'importe quelle IP de la machine".     */
    adresse_ecoute.sin_addr.s_addr = 0;

    /* On lie la socket a l'adresse qu'on vient de configurer avec bind().  */
    if ( bind(  socket_passive
                 , (struct sockaddr *) &adresse_ecoute
                 , sizeof( adresse_ecoute ) )
         == -1 )
    {
        perror( "bind()" );
        exit( -1 );
    }

    /* On dit au systeme que cette socket est "passive" , qu'elle ecoute.   *
     * Trois clients au maximum seront en attente de connexion, s'il y en a *
     * un quatrieme, il sera rejete.                                        */
    if ( listen( socket_passive, 3 ) == -1 )
    {
        perror( "listen()" );
        exit( -1 );
    }

    taille = sizeof( struct sockaddr_in );


    /* On traite nos clients dans une boucle infinie.                       */
    while ( 1 )
    {
	        /* On prend un nouveau client. Cela nous cree une nouvelle socket,  *
	         * juste pour lui. Au passage, on attrape son adresse.              */
	        socket_client = accept(  socket_passive
	                               , (struct sockaddr *) &adresse_client
	                               , &taille );
	        if ( socket_client == -1 )
	        {
	            perror( "accept()" );
	            exit( -1 );
	        }

	        printf(  "Un client s'est connecté %s : %d\n"
	               , inet_ntop(  AF_INET
	                           , &(adresse_client.sin_addr)
	                           , buffer_adresse
	                           , 128 )
	               , adresse_client.sin_port );


	        // C'est plus propre, on ne sait pas a quoi ressemble le message.   
	        memset( buffer_message, 0, sizeof( buffer_message ) );
	        taille_recue = recv( socket_client, buffer_message, TAILLE_MESSAGE, 0 );
	        
	        if ( taille_recue == -1 )
	        {
	            perror( "recv()" );
	            exit( -1 );
	        }

	        // Au pire on affichera n'importe quoi, sans deborder du buffer.    
	        //buffer_message[999] = '\0'; 

	        printf(  "%s\n", buffer_message );


	       	/*taille_envoyee = send(  socket_client
		                          , buffer_message 
		                          , strlen(buffer_message)
		                          , 0 );

		    if ( taille_envoyee == -1 )
		    {
		        perror( "send()" );
		        exit( -1 );
		    }*/
		
		
        // A ne pas oublier.                                                 
        close( socket_client );
    }

    // A ne pas oublier.                                                   
    close( socket_passive );

    return 0;
}
