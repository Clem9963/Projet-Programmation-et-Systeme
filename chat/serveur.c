#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

 #define NB_CLIENT_MAX 3

void ouvertureServeur(int sock);
void ecouteConnexion(int csock, char *buffer);
void envoiMessage(int csock, char *buffer);
void ecouteMessage(int *listeSock, int indice, char *buffer, int nbClients);

int main(){
	
	int sockServeur = socket(AF_INET, SOCK_STREAM, 0); /* créé un socket TCP */
	char buffer[1000];
	fd_set readfds;
	socklen_t taille;
	struct sockaddr_in csin; 
	int nbClients = 0;
	int i;
	int *listeSock = malloc(sizeof(int) * NB_CLIENT_MAX);
	
	ouvertureServeur(sockServeur);
	
	taille = sizeof( struct sockaddr_in );
	
	
	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sockServeur, &readfds);
		for(i = 0; i < nbClients;i++)
			FD_SET(listeSock[i], &readfds);	
		
		if(select(sockServeur + nbClients + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			exit(-1);
		}
				
		if(FD_ISSET(sockServeur, &readfds))
		{
			listeSock[nbClients] = accept(sockServeur, (struct sockaddr *)&csin, &taille);
			ecouteConnexion(listeSock[nbClients], buffer);
			nbClients++;
		}
		else
		{
			
			for (i = 0; i < nbClients; i++)
			{
				if(FD_ISSET(listeSock[i], &readfds))
				{
					ecouteMessage(listeSock, i, buffer, nbClients);
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
	sin.sin_port = htons(54888);
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
}

void ecouteConnexion(int csock, char *buffer){
	ssize_t taille_recue;
	char *bienvenue = malloc(sizeof(char) * (strlen(buffer) + 10)); 

	taille_recue = recv(csock, buffer, 1000, 0);
	
	if(taille_recue == -1)
	{
		perror("recv()");
		exit(-1);
	}
	else
	{
		buffer[taille_recue] = '\0';
		printf("%s est connecté\n",buffer);
		sprintf(bienvenue, "bienvenue %s", buffer);
		envoiMessage(csock, bienvenue);//message de bienvenue
		free(bienvenue);
	}
}

void envoiMessage(int csock, char *buffer){
	if(send(csock, buffer, strlen(buffer), 0) == -1)
	{
		perror("send()");
		exit(-1);
	}
}

void ecouteMessage(int *listeSock, int indice, char *buffer, int nbClients){
	ssize_t taille_recue;
	int i;

	taille_recue = recv(listeSock[indice], buffer, 1000, 0);
	
	if(taille_recue == -1)
	{
		perror("recv()");
		exit(-1);
	}
	else
	{
		buffer[taille_recue] = '\0';
		printf("%s\n", buffer);
		for (i = 0; i < nbClients; i++)
		{
			if(i != indice)
				envoiMessage(listeSock[i], buffer);//retransmet le message aux autres
		}
	}
}









