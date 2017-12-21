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

struct Client
{
	char pseudo[TAILLE_PSEUDO];
	int csock;
};

void ouvertureServeur(int sock);
void ecouteConnexion(int csock, char *buffer, char *pseudo);
void envoiMessage(int csock, char *buffer);
void ecouteMessage(struct Client *clients, int indice, char *buffer, int *nbClients);
void deconnexionClient(struct Client *clients, int indice, int *nbClients);
void envoiMessageTous(struct Client *clients, char *buffer, int *nbClients);
void envoiMessageAutresClients(struct Client *clients, int indice, char *buffer, int *nbClients);
void concatener(char *buffer, char *pseudo);