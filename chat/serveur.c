#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>


int main(void){
	int socket_passive;
	int socket_client;
	struct sockaddr_in adresse_ecoute;
	struct sockaddr_in adresse_client;
	char buffer_adresse[128];
	socklen_t taille;
	ssize_t taille_recue;
	char buffer_message[128];

	socket_passive = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_passive == -1)
	{
		perror("socket()");
		exit(-1);
	}
	memset(&adresse_ecoute, 0, sizeof(adresse_ecoute));
	adresse_ecoute.sin_family = AF_INET;

	adresse_ecoute.sin_port = htons(54878);

	adresse_ecoute.sin_addr.s_addr = 0;

	if(bind(socket_passive, (struct sockaddr *) &adresse_ecoute, sizeof(adresse_ecoute)) == -1)
	{
		perror("bind()");
		exit(-1);
	}

	if(listen(socket_passive, 3) == -1)
	{
		perror("listen()");
		exit(-1);
	}

	taille = sizeof(struct sockaddr_in);

	while(1)
	{
		socket_client = accept(socket_passive, (struct sockaddr *) &adresse_client, &taille);
		if (socket_client == -1)
		{
			perror("accept()");
			exit(-1);
		}
		printf("Un client s'est connecté..%s : %d\n", inet_ntop(AF_INET, &(adresse_client.sin_addr),buffer_adresse, 128), adresse_client.sin_port);
		memset(buffer_message, 0, sizeof(buffer_message));
		taille_recue = recv(socket_client, buffer_message, 128, 0);
		if (taille_recue == -1)
		{
			perror("recv()");
			exit(-1);
		}	
		buffer_message[127] = '\0';
		printf("message recu (taille %ld) : \n[%s]\n",taille_recue, buffer_message);
		close(socket_client);

	}
	close(socket_passive);
	return 0;
}