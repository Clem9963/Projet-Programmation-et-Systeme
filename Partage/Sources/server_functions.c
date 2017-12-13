#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "server_functions.h"

#define h_addr h_addr_list[0]
#define SOCKET_ERROR -1			/* code d'erreur des sockets  */

void getDirectory(char *path, size_t length)
{
	FILE *f = NULL;
	int bouncer = 0;
	char *char_ptr = NULL;

	while (f == NULL)
	{
		printf("Veuillez entrer le chemin d'un fichier s'il-vous-plaît (relatif ou absolu)\n");
		if (fgets(path, length, stdin) == NULL)
		{
			perror("fgets error");
			exit(errno);
		}

		char_ptr = strchr(path, '\n');
		if (char_ptr != NULL)
		{
			*char_ptr = '\0';
		}
		else
		{
			while (bouncer != '\n' && bouncer != EOF)
			{
				bouncer = getchar();
			}
		}
		printf("%s\n", path);

		f = fopen(path, "r");
		if (f == NULL)
		{
			perror("fopen error");
			exit(errno);
		}
		else
		{
			printf("Fichier ouvert avec succès\n");
			fclose(f);
		}
	}
}

int init_server(int port)
{
	return 0;
}