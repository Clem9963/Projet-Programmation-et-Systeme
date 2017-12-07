#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define NB_CLIENT_MAX 3
#define PORT 54888
#define TAILLE_BUF 1000
#define TAILLE_PSEUDO 16

void ouvertureServeur(int sock);
void ecouteConnexion(int csock, char *buffer, int indice, char **listePseudo);
void envoiMessage(int csock, char *buffer);
void ecouteMessage(int *listeSock, int indice, char *buffer, int *nbClients, char **listePseudo);
void deconnexionClient(int * listeSock, int indice, int *nbClients, char **listePseudo);
void envoiMessageTous(int *listeSock, char *buffer, int *nbClients);
void envoiMessageAutresClients(int *listeSock, int indice, char *buffer, int *nbClients);
void concatener(char *buffer, char *pseudo);