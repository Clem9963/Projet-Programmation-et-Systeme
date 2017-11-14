#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main (int argc, char * argv[]){
	int socket_connexion;
	struct sockaddr_in adresse_serveur;
	char buffer_adresse[128];
	ssize_t taille_envoyee;
	if (argc != 2)
	{
		fprintf(stderr, "donnez un argument a envoyer ....\n");
		exit(-1);
	}
	if (strlen(argv[1]) >=128)
	{
		fprintf(stderr, "Votre argument est trop long...\n");
		exit(-1);
	}

	socket_connexion = socket(AF_INET, SOCK_STREAM,0);
	if (socket_connexion == -1)
	{
		perror("socket()");
		exit(-1);
	}

	memset(&adresse_serveur, 0, sizeof(adresse_serveur));
	adresse_serveur.sin_family = AF_INET;

	adresse_serveur.sin_port = htons(54878);

	inet_pton(AF_INET, "192.168.1.23", &(adresse_serveur.sin_addr.s_addr));

	if (connect(socket_connexion,(const struct sockaddr *) &adresse_serveur, sizeof(adresse_serveur)) == -1)
	{
		perror("connect()");
		exit(-1);
	}
	printf("Connexion a %s : %d reussit\n",inet_ntop(AF_INET, &(adresse_serveur.sin_addr), buffer_adresse, 128), adresse_serveur.sin_port);

	taille_envoyee = send(socket_connexion, argv[1], strlen(argv[1]), 0);
	if (taille_envoyee == -1)
	{
		perror("send()");
		exit(-1);
	}
	printf("message envoye\n");
	close(socket_connexion);
	return 0;


}