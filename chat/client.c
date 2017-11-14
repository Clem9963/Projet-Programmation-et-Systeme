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

int main( int argc, char * argv[] )
{
    int                socket_connexion;
    struct sockaddr_in adresse_serveur;
    char               buffer_adresse[128];
    ssize_t            taille_envoyee;

    if ( argc != 2 )
    {
        fprintf( stderr, "Donnez un argument a envoyer...\n" );
        exit( -1 );
    }
    if ( strlen( argv[1] ) >= 127 )
    {
        fprintf( stderr, "Votre argument est trop long...\n" );
        exit( -1 );
    }

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
    inet_pton( AF_INET, "127.0.0.1", &(adresse_serveur.sin_addr.s_addr) );

    if (   connect(  socket_connexion
                   , (const struct sockaddr *) &adresse_serveur
                   , sizeof( adresse_serveur ) )
        == -1 )
    {
        perror( "connect()" );
        exit( -1 );
    }

    printf(  "Connexion a %s : %d reussie\n"
           , inet_ntop(  AF_INET                     /* Pour transformer la */
                       , &(adresse_serveur.sin_addr) /* bouillie d'octets   */
                       , buffer_adresse              /* en quelque chose    */
                       , 128 )                       /* de lisible.         */
           , adresse_serveur.sin_port );

    /* On envoie notre message. Ici c'est le premier argument passe au      *
     * programme. C'etait pas si complique, si ?                            */
    taille_envoyee = send(  socket_connexion
                          , argv[1] 
                          , strlen( argv[1] )
                          , 0 );

    if ( taille_envoyee == -1 )
    {
        perror( "send()" );
        exit( -1 );
    }

    printf( "message envoye\n" );

    /* A ne pas oublier (moi j'avais oublie, ca m'a embete)                 */
    close( socket_connexion );

    return 0;
}
