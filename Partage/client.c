#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_functions.h"

#define TRUE 1
#define FALSE 0

int main(int argc, char *argv[])
{
	/* La fonction main attend 3 paramètres :
	-> Le pseudo de l'utilisateur
	-> L'adresse IP du serveur
	-> Le port du serveur */
	char *username = NULL;
	char *address = NULL;
	int port = 0;

	char path[512] = "";

	if (argc != 4)
	{
		fprintf(stderr, "Nombre d'arguments fournis incorrect\n");
	}

	username = argv[1];
	if (strlen(username) > 15)
	{
		username[15] = '\0';	// Pas de pseudo trop long !
	}
	address = argv[2];
	port = atoi(argv[3]);

	getDirectory(path, sizeof(path));

	printf("path : %s\n", path);
	printf("port : %d\n", port);

	return EXIT_SUCCESS;
}
