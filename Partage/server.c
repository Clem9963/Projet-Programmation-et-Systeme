#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server_functions.h"

#define TRUE 1
#define FALSE 0

int main(int argc, char *argv[])
{
	int port = 0;
	char path[512] = "";

	if (argc != 2)
	{
		fprintf(stderr, "Nombre d'arguments fournis incorrect\n");
	}

	port = atoi(argv[1]);
	getDirectory(path, sizeof(path));

	printf("path : %s\n", path);
	printf("port : %d\n", port);

	return EXIT_SUCCESS;
}
