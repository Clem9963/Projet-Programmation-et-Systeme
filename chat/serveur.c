#include "serveur.h"

int main(){

	//initialisation des variables
	int i, succes = 0;
	int nbClients = 0;
	int sockServeur = socket(AF_INET, SOCK_STREAM, 0); //Création d'un socket TCP
	struct Client clients[NB_CLIENT_MAX];
	char buffer[TAILLE_BUF];
	char *pseudo;
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
			FD_SET(clients[i].csock, &readfds);	
		
		if(select(sockServeur + nbClients + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			close(sockServeur);
			exit(-1);
		}
		
		//ecoute une connexion de client
		if(FD_ISSET(sockServeur, &readfds))
		{
			clients[nbClients].csock = accept(sockServeur, (struct sockaddr *)&csin, &taille);
			ecouteConnexion(clients[nbClients].csock, buffer,clients[nbClients].pseudo);
			
			//message de connexion pour les autres clients
			sprintf(buffer, "%s est connecté", clients[nbClients].pseudo);

			//envoi un message aux autres pour dire la connexion d'un client
			envoiMessageAutresClients(clients, nbClients, buffer, &nbClients);
		
			nbClients++;
		}
		//ecoute de l'entrée standart
		else if(FD_ISSET(STDIN_FILENO, &readfds))
		{
			//recupere le message
			fgets(buffer, TAILLE_BUF, stdin);
			//si /quit on quitte le serveur
			if(strncmp(buffer, "/quit", 5) == 0)
			{
				close(sockServeur);
				printf("\n\nDéconnexion réussie\n");
				exit(-1);
			}
			//si /kick ... on déconnecte le client demandé
			else if(strncmp(buffer, "/kick", 5) == 0)
			{
				//pour récuperer le pseudo rentré en deuxième parametre
				pseudo = strtok(buffer, " ");
				pseudo = strtok(NULL, " ");
				if (pseudo != NULL)
				{	
					succes = 0;
					pseudo[strlen(pseudo) - 1] = '\0';
					//on cherche le client en comparant avec la liste des pseudos
					for(i = 0; i < nbClients; i++)
					{
						if(strcmp(clients[i].pseudo, pseudo) == 0)
						{
							deconnexionClient(clients, i, &nbClients);
							printf("%s déconnecté avec succès \n", pseudo);
							succes = 1;
						}
					}

					//verifie que le client a bien été deconnecté
					if(succes == 0)
						printf("Le client n'existe pas !\n");
				}
				else
					printf("Veuillez rentré un pseudo !\n");
			}
			//si /list on liste les clients connectés
			else if(strncmp(buffer, "/list", 5) == 0)
			{
				printf("\nVoici les clients connectés : \n");
				for (i = 0; i < nbClients; i++)
					printf("%s\n", clients[i].pseudo);
			}
			//sinon on envoie aux clients
			else
			{
				concatener(buffer, "Serveur");
				envoiMessageTous(clients, buffer, &nbClients);
			}
		}
		//ecoute les sockets clients
		else
		{
			for (i = 0; i < nbClients; i++)
			{
				if(FD_ISSET(clients[i].csock, &readfds))
				{
					ecouteMessage(clients, i, buffer, &nbClients);
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
void ecouteConnexion(int csock, char *buffer, char *pseudo){
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
		strcpy(pseudo, buffer);
	}
}

void envoiMessage(int csock, char *buffer){
	if(send(csock, buffer, strlen(buffer), 0) == -1)
	{
		perror("send()");
		exit(-1);
	}
}

void envoiMessageTous(struct Client *clients, char *buffer, int *nbClients){
	int i;
	for (i = 0; i < *nbClients; i++)
		envoiMessage(clients[i].csock, buffer); 
}

void envoiMessageAutresClients(struct Client *clients, int indice, char *buffer, int *nbClients){
	int i;
	for (i = 0; i < *nbClients; i++)
	{
		if(i != indice)
			envoiMessage(clients[i].csock, buffer);//retransmet le message aux autres
	}
}

void ecouteMessage(struct Client *clients, int indice, char *buffer, int *nbClients){
	ssize_t taille_recue;
	int i;

	taille_recue = recv(clients[indice].csock, buffer, TAILLE_BUF, 0); //recoi le message
	
	//gère une erreur
	if(taille_recue == -1)
	{
		perror("recv()");
		exit(-1);
	}
	//pour gerer la deconnexion du client
	else if(taille_recue == 0)
	{
		sprintf(buffer, "%s s'est déconnecté", clients[indice].pseudo);
		envoiMessageAutresClients(clients, indice, buffer, nbClients);
		printf("%s\n", buffer);
		deconnexionClient(clients, indice, nbClients);
	}
	//envoi le message aux autres clients si pas d'erreurs
	else
	{
		buffer[taille_recue] = '\0';

		//verifie que le message n'est pas la commande "/list"
		//si c'est "/list" alors on envoit les pseudos des personnes 
		//connectés au client qui les a demandé
		if(strcmp(buffer, "/list") == 0)
		{
			printf("%s : /list\n", clients[indice].pseudo);
			strcpy(buffer, "Les clients connectés sont : ");
			for (i = 0; i < *nbClients; i++)
			{
				if(i != indice)
					strcat(buffer, clients[i].pseudo);
				else
					strcat(buffer, "vous");
					
				strcat(buffer, ", ");
			}
			envoiMessage(clients[indice].csock, buffer);
		}
		else
		{
			concatener(buffer, clients[indice].pseudo);
			printf("%s\n", buffer);
			envoiMessageAutresClients(clients, indice, buffer, nbClients);
		}
	}
}

//deconnexion d'un client
void deconnexionClient(struct Client *clients, int indice, int *nbClients){
	int i;

	if(close(clients[indice].csock) == -1)// ferme le socket
	{
		perror("close()");
		exit(-1);
	}
	clients[indice].csock = 0;
	strcpy(clients[indice].pseudo, "");

	(*nbClients)--;// diminue le nombre de clients

	// refaire la listeSock et listepseudo bien comme il faut
	for(i = indice; i < *nbClients; i++)
	{
		clients[i].csock = clients[i + 1].csock;
		strcpy(clients[i].pseudo, clients[i + 1].pseudo);
		clients[i + 1].csock = 0;
		strcpy(clients[i + 1].pseudo, "");
	}
}

//concatenation du pseudo devant le message
void concatener(char *buffer, char *pseudo){
	char temp[TAILLE_BUF + TAILLE_PSEUDO];
	sprintf(temp, "%s : %s", pseudo, buffer);
	strcpy(buffer, temp);
}