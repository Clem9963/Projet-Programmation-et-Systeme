#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define TAILLE_BUF 1000
#define TAILLE_PSEUDO 16
#define PORT 54888

char *demandeIP();
char *demandePseudo();
int connect_socket(char *adresse, int port);
void read_serveur(int sock, char *buffer);
void write_serveur(int sock, char *buffer);

int main(int argc, char *argv[]){
	char buffer[TAILLE_BUF];
	int sock;
	char * pseudo = demandePseudo();
	char *ipAdresse = demandeIP();
	fd_set readfds;	
	
	sock = connect_socket(ipAdresse, PORT);	
	write_serveur(sock, pseudo); //envoi le pseudo au serveur

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(sock, &readfds);
		if(select(sock + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			exit(-1);
		}
		
		if(FD_ISSET(sock, &readfds))
		{
			read_serveur(sock, buffer);
		}
		else if(FD_ISSET(STDIN_FILENO, &readfds))
		{
			fgets(buffer,sizeof(buffer),  stdin);
			buffer[strlen(buffer) - 1] = '\0';
			write_serveur(sock, buffer);
		}
	}

	close(sock);
	return 0;
}

int connect_socket(char *adresse, int port){
	int sock = socket(AF_INET, SOCK_STREAM, 0); /* créé un socket TCP */

	if( sock == -1)
	{
		perror("socket()");
		exit(-1);
	}

	struct hostent* hostinfo = gethostbyname(adresse); /* infos du serveur */
	if(hostinfo == NULL)
	{
		perror("gethostbyname()");
		exit(-1);
	}
	
	struct sockaddr_in sin; /* structure qui possède toutes les infos pour le socket */
	sin.sin_addr = *(struct in_addr*) hostinfo->h_addr; /* on spécifie l'addresse */
	sin.sin_port = htons(port); /* le port */
	sin.sin_family = AF_INET; /* et le protocole (AF_INET pour IP) */
	
	if(connect(sock, (struct sockaddr*) &sin, sizeof(struct sockaddr)) == -1)
	{
		perror("connect()");
		exit(-1);
	} /* demande de connection */
	return sock;
}

void read_serveur(int sock, char *buffer){
	int taille_recue;
	taille_recue = recv(sock, buffer, TAILLE_BUF, 0);

	if(taille_recue == -1)
	{
		perror("recv()");
		exit(-1);
	}
	//pour gerer la deconnexion du serveur
	else if(taille_recue == 0)
	{
		printf("la connexion au serveur a été interrompue\n");
		exit(-1);
	}
	else
	{
		buffer[taille_recue] = '\0';
		printf("%s\n",buffer);
	}	
}

void write_serveur(int sock, char *buffer){
	if(send(sock, buffer, strlen(buffer), 0) == -1)
	{
		perror("send()");
		exit(-1);
	}
}

char *demandeIP(){
	char *adresseIP = malloc(sizeof(char) * 16);

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
	char *pseudo = malloc(sizeof(char) * 16);

	printf("\nVotre pseudo : ");
    scanf("%s",pseudo);

    while(strlen(pseudo) > 15)
    {
    	printf("\nVotre pseudo est trop long (au max 15 caracteres)");
    	printf("\nVotre pseudo : ");
    	scanf("%s",pseudo);
    }

    return pseudo;
}