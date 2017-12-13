#include "serveur.h"

int main(){

	//initialisation des variables
	int i;
	int nbClients = 0;
	int sockServeur = socket(AF_INET, SOCK_STREAM, 0); //Création d'un socket TCP
	int listeSock[NB_CLIENT_MAX];
	char listePseudo[NB_CLIENT_MAX][TAILLE_PSEUDO];	
	char buffer[TAILLE_BUF];
	struct sockaddr_in csin; 
	fd_set readfds;
	socklen_t taille = sizeof(struct sockaddr_in);

	//demarrage du serveur
	ouvertureServeur(sockServeur);
	
	//boucle d'ecoute
	while(1)
	{
		FD_ZERO(&readfds); //met à 0 le readfds

		//remplit le readfds
		FD_SET(sockServeur, &readfds); 
		FD_SET(STDIN_FILENO, &readfds);

		for(i = 0; i < nbClients; i++)
			FD_SET(listeSock[i], &readfds);	
		
		if(select(sockServeur + nbClients + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			close(sockServeur);
			exit(-1);
		}
		
		//ecoute une connexion de client
		if(FD_ISSET(sockServeur, &readfds))
		{
			listeSock[nbClients] = accept(sockServeur, (struct sockaddr *)&csin, &taille);
			ecouteConnexion(listeSock[nbClients], buffer, nbClients, listePseudo);
			
			//message de connexion pour les autres clients
			sprintf(buffer, "%s est connecté", listePseudo[nbClients]);

			//envoi un message aux autres pour dire la connexion d'un client
			envoiMessageAutresClients(listeSock, nbClients, buffer, &nbClients);
		
			nbClients++;
		}
		//ecoute de l'entrée standart
		else if(FD_ISSET(STDIN_FILENO, &readfds))
		{
			//recupere le message et l'envoi message a tout le monde
			fgets(buffer, TAILLE_BUF, stdin);
			concatener(buffer, "Serveur");
			envoiMessageTous(listeSock, buffer, &nbClients);
		}
		//ecoute les sockets clients
		else
		{
			for (i = 0; i < nbClients; i++)
			{
				if(FD_ISSET(listeSock[i], &readfds))
				{
					ecouteMessage(listeSock, i, buffer, &nbClients, listePseudo);
				}
			}
		}
	}
	
	//ferme le socket
	close(sockServeur);

	return 0;
}

//lancement du serveur
void ouvertureServeur(int sock){
	struct sockaddr_in sin;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(PORT);
	sin.sin_family = AF_INET;

	if(bind (sock, (struct sockaddr*) &sin, sizeof(sin)) == -1)
	{
		perror("bind()");
		exit(-1);
	}
	
	//ecoute les connexions
	if(listen(sock, NB_CLIENT_MAX) == -1) 
	{
		perror("listen");
		exit(-1);
	}
	printf("Le serveur est opérationnel\n");
}

//ecoute la connexion d'un client
void ecouteConnexion(int csock, char *buffer, int indice, char listePseudo[NB_CLIENT_MAX][TAILLE_PSEUDO]){
	ssize_t taille_recue;
	char bienvenue[strlen(buffer) + 10]; //+10 car faut rajouter le "Bienvenue "

	taille_recue = recv(csock, buffer, TAILLE_BUF, 0); //recoit le pseudo
	
	//gère une erreur
	if(taille_recue == -1)
	{
		close(csock);
		perror("recv()");
		exit(-1);
	}
	else
	{
		buffer[taille_recue] = '\0';
		printf("%s est connecté\n", buffer);
		sprintf(bienvenue, "Bienvenue %s", buffer);
		envoiMessage(csock, bienvenue);//message de bienvenue

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

void ecouteMessage(int *listeSock, int indice, char *buffer, int *nbClients, char listePseudo[NB_CLIENT_MAX][TAILLE_PSEUDO]){
	ssize_t taille_recue;
	int i;

	taille_recue = recv(listeSock[indice], buffer, TAILLE_BUF, 0); //recoi le message
	
	//gère une erreur
	if(taille_recue == -1)
	{
		perror("recv()");
		exit(-1);
	}
	//pour gerer la deconnexion du client
	else if(taille_recue == 0)
	{
		sprintf(buffer, "%s s'est déconnecté", listePseudo[indice]);
		envoiMessageAutresClients(listeSock, indice, buffer, nbClients);
		printf("%s\n", buffer);
		deconnexionClient(listeSock, indice, nbClients, listePseudo);
	}
	//envoi le message aux autres clients si pas d'erreurs
	else
	{
		buffer[taille_recue] = '\0';
		concatener(buffer, listePseudo[indice]);
		printf("%s\n", buffer);
		envoiMessageAutresClients(listeSock, indice, buffer, nbClients);
	}
}

//deconnexion d'un client
void deconnexionClient(int *listeSock, int indice, int *nbClients, char listePseudo[NB_CLIENT_MAX][TAILLE_PSEUDO]){
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
			for (j = i + 1; j < *nbClients; j++)
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

//concatenation du pseudo devant le message
void concatener(char *buffer, char *pseudo){
	char temp[TAILLE_BUF + TAILLE_PSEUDO];
	sprintf(temp, "%s : %s", pseudo, buffer);
	strcpy(buffer, temp);
}