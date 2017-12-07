#include "serveur.h"

int main(){
	int i;
	int nul = 0;
	int *nbClients = &nul;
	int sockServeur = socket(AF_INET, SOCK_STREAM, 0); /* créé un socket TCP */
	int *listeSock = malloc(sizeof(int) * NB_CLIENT_MAX);

	char **listePseudo = malloc(sizeof(char *) * NB_CLIENT_MAX);	
    for (i = 0; i < NB_CLIENT_MAX; i++)
        listePseudo[i] = malloc(sizeof(char)* 16);

	char buffer[TAILLE_BUF];
	struct sockaddr_in csin; 
	fd_set readfds;
	socklen_t taille = sizeof( struct sockaddr_in );

	//demarrage du serveur
	ouvertureServeur(sockServeur);
	
	//boucle d'ecoute
	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sockServeur, &readfds);
		FD_SET(STDIN_FILENO, &readfds);

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
		//ecoute de l'entrée standart
		else if(FD_ISSET(STDIN_FILENO, &readfds))
		{
			//recupere le message et l'envoi message a tout le monde
			fgets(buffer, TAILLE_BUF,  stdin);
			concatener(buffer, "Serveur");
			envoiMessageTous(listeSock, buffer, nbClients);
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

void envoiMessageTous(int *listeSock, char *buffer, int *nbClients){
	int i;
	for (i = 0; i < *nbClients; i++)
		envoiMessage(listeSock[i], buffer); 
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