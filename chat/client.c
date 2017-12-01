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

int connect_socket(char *adresse, int port);
void read_serveur(int sock, char *buffer);
void write_serveur(int sock, char *buffer);

int main(int argc, char *argv[]){
	char buffer[TAILLE_BUF];
	int sock;
	char * pseudo = argv[1];
	fd_set readfds;

	if(argc != 4)
	{
		printf("\nl'appel doit etre du style ./client pseudo ip port \n");
		exit(-1);
	}		
	
	sock = connect_socket(argv[2], atoi(argv[3]));	
	write_serveur(sock, pseudo);

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
			printf("Vous : %s\n", buffer);
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