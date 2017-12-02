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
void envoiMessageAutresClients(int *listeSock, int indice, char *buffer, int *nbClients);
void concatener(char *buffer, char *pseudo);

int main(){
	
	int sockServeur = socket(AF_INET, SOCK_STREAM, 0); /* créé un socket TCP */
	char buffer[TAILLE_BUF];
	fd_set readfds;
	socklen_t taille;
	struct sockaddr_in csin; 
	
	int nul = 0;
	int *nbClients = &nul;

	int i;
	int *listeSock = malloc(sizeof(int) * NB_CLIENT_MAX);

	char **listePseudo = malloc(sizeof(char *) * NB_CLIENT_MAX);	
    for (i = 0; i < NB_CLIENT_MAX; i++)
        listePseudo[i] = malloc(sizeof(char)* 16);
	
	//demarrage du serveur
	ouvertureServeur(sockServeur);
	
	taille = sizeof( struct sockaddr_in );
	
	//boucle d'ecoute
	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sockServeur, &readfds);
		for(i = 0; i < *nbClients;i++)
			FD_SET(listeSock[i], &readfds);	
		
		if(select(sockServeur + (*nbClients) + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			exit(-1);
		}
		
		//ecoute une connexion de client
		if(FD_ISSET(sockServeur, &readfds))
		{
			listeSock[*nbClients] = accept(sockServeur, (struct sockaddr *)&csin, &taille);
			ecouteConnexion(listeSock[*nbClients], buffer, *nbClients, listePseudo);
			
			//message de connexion pour les autres clients
			sprintf(buffer, "%s est connecté", listePseudo[*nbClients]);

			//envoi un message aux autres pour dire la connexion d'un client
			envoiMessageAutresClients(listeSock, *nbClients, buffer, nbClients);
		
			(*nbClients)++;
		}
		//ecoute les sockets clients
		else
		{
			for (i = 0; i < *nbClients; i++)
			{
				if(FD_ISSET(listeSock[i], &readfds))
				{
					ecouteMessage(listeSock, i, buffer, nbClients, listePseudo);
				}
			}
		}
	}
	
	close(sockServeur);

	return 0;
}


void ouvertureServeur(int sock){
	struct sockaddr_in sin;
	sin.sin_addr.s_addr = htonl(INADDR_ANY); /* on accepte toute adresse */
	sin.sin_port = htons(PORT);
	sin.sin_family = AF_INET;
	if(bind (sock, (struct sockaddr*) &sin, sizeof(sin)) == -1) /* on lie le socket à sin */
	{
		perror("bind()");
		exit(-1);
	}
	
	if(listen(sock, NB_CLIENT_MAX) == -1) /* notre socket est prêt à écouter une connection */
	{
		perror("listen");
		exit(-1);
	}
	printf("Le serveur est opérationnel\n");
}

void ecouteConnexion(int csock, char *buffer, int indice, char **listePseudo){
	ssize_t taille_recue;
	char *bienvenue = malloc(sizeof(char) * (strlen(buffer) + 10)); 

	taille_recue = recv(csock, buffer, TAILLE_BUF, 0);
	
	if(taille_recue == -1)
	{
		close(csock);
		perror("recv()");
		exit(-1);
	}
	else
	{
		buffer[taille_recue] = '\0';
		printf("%s est connecté\n",buffer);
		sprintf(bienvenue, "Bienvenue %s", buffer);
		envoiMessage(csock, bienvenue);//message de bienvenue
		free(bienvenue);

		//isncrire dans la liste des pseudos
		strcpy(listePseudo[indice], buffer);
	}
}

void envoiMessage(int csock, char *buffer){
	if(send(csock, buffer, strlen(buffer), 0) == -1)
	{
		perror("send()");
		exit(-1);
	}
}

void envoiMessageAutresClients(int *listeSock, int indice, char *buffer, int *nbClients){
	int i;
	for (i = 0; i < *nbClients; i++)
	{
		if(i != indice)
			envoiMessage(listeSock[i], buffer);//retransmet le message aux autres
	}
}

void ecouteMessage(int *listeSock, int indice, char *buffer, int *nbClients, char **listePseudo){
	ssize_t taille_recue;
	int i;

	taille_recue = recv(listeSock[indice], buffer, TAILLE_BUF, 0);
	
	if(taille_recue == -1)
	{
		perror("recv()");
		exit(-1);
	}
	//pour gerer la deconnexion directe du client
	else if(taille_recue == 0)
	{
		sprintf(buffer, "%s s'est déconnecté", listePseudo[indice]);
		envoiMessageAutresClients(listeSock, indice, buffer, nbClients);
		printf("%s\n",buffer);
		deconnexionClient(listeSock, indice, nbClients, listePseudo);
	}

	else
	{
		buffer[taille_recue] = '\0';
		concatener(buffer, listePseudo[indice]);
		printf("%s\n", buffer);
		envoiMessageAutresClients(listeSock, indice, buffer, nbClients);
	}
}

void deconnexionClient(int * listeSock, int indice, int *nbClients, char **listePseudo){
	int i, j;

	if(close(listeSock[indice]) == -1)// ferme le socket
	{
		perror("close()");
		exit(-1);
	}
	listeSock[indice] = 0;
	strcpy(listePseudo[indice], "");

	// refaire la listeSock et listepseudo bien comme il faut
	for(i = 0; i < *nbClients; i++)
	{
		if(listeSock[i] == 0)
		{
			for (j=i+1;j<*nbClients;j++)
			{
				if(listeSock[j] != 0)
				{
					listeSock[i] = listeSock[j];
					listeSock[j] = 0;
					strcpy(listePseudo[i], listePseudo[j]);
					strcpy(listePseudo[j], "");
				}
				break;
			}
		}
	}

	(*nbClients)--;// diminue le nombre de clients
}

void concatener(char *buffer, char *pseudo){
	char *temp = malloc(sizeof(char) * (TAILLE_BUF + TAILLE_PSEUDO));
	sprintf(temp, "%s : %s", pseudo, buffer);
	strcpy(buffer, temp);
	free(temp);
}