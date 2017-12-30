#include "server.h"
#include "server_functions.h"

int listenSocket(int port)
{
	/* Fonction retournant une socket prête à écouter des connexions sur le port fourni en argument */

	int passive_sock = socket(AF_INET, SOCK_STREAM, 0);		// Création de la socket
	struct sockaddr_in sin; 								// Structure qui possède toutes les infos pour la socket
	memset(&sin, 0, sizeof(sin));							// Initialisation de la structure en mettant tous les bits à 0
	
	if (passive_sock == SOCKET_ERROR)
	{
		perror("< FERROR > socket error");
		exit(errno);
	}

	sin.sin_addr.s_addr = htonl(INADDR_ANY);	// On accepte toute adresse
	sin.sin_port = htons(port);					// Le port
	sin.sin_family = AF_INET;					// et le protocole (AF_INET pour IP)

	if (bind(passive_sock, (struct sockaddr*) &sin, sizeof(sin)) == SOCKET_ERROR) // Tentative de liage de la socket à sin
	{
		perror("< FERROR > bind error");
		exit(errno);
	}

	if (listen(passive_sock, 1) == SOCKET_ERROR)	// Notre socket est prête à écouter une connexion à la fois
	{
    	perror("< FERROR > listen error");
    	exit(errno);
	}

	return passive_sock;
}

int getClient(struct Client *clients, int clients_nb, char *buffer)
{
	/* Fonction retournant l'indice du client dont l'username est celui de la requête "/sendto"
	La fonction retourne -1 s'il n'y a aucun client correspondant
	Elle prend en arguments le tableau des clients, le nombre de clients actifs
	et la requête "/sendto" dans buffer qui est une chaîne de caractères */

	char username[BUFFER_SIZE] = "";	// Il est obligatoire de créer une variable intermédiaire de taille BUFFER_SIZE car elle va recevoir une copie de la requête
	int found = FALSE;						
	int i = 0;

	strcpy(username, strchr(buffer, ' ') + 1);
	*(strchr(username, ' ')) = '\0';
	while (!found && i < clients_nb)
	{
		found = !strcmp(username, clients[i].username);
		i++;
	}

	if (found)
	{
		i--;
		return i;
	}
	else
	{
		return -1;
	}
}

void *transferControl(void *src_data)
{
	/* Fonction gérant le transit du fichier par le serveur.
	Elle prend en arguments un pointeur sur void qui sera casté par
	la suite en un pointeur sur une structure struct TransferDetails.
	La fonction retourne toujours NULL */

	struct TransferDetails *data = (struct TransferDetails *)src_data;
	char buffer[BUFFER_SIZE] = "";		// Buffer intermédiaire permmettant de recevoir et transmettre les paquets de l'émetteur vers le destinataire
	char *char_ptr = NULL;				// Pointeur permettant la recherche d'un caractère
	int answer = 0;						// Entier pour stocker la réponse du client destinataire
	int package_number = 0;				// Nombre de paquets de 1 024 octets
	int residue_size = 0;				// Taille du dernier paquet
	int i = 0;

	pthread_mutex_lock(data->mutex_thread_status);	// Cette fonction est bloquante jusqu'à ce que le mutex puisse être vérouillé

	sendClient(data->receiving_client.msg_client_sock, data->request, strlen(data->request)+1);	// Envoi de la requête au destinataire
	recvClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));				// Réception de la réponse de ce dernier (0 : Refus, 1 : Accord)

	answer = atoi(buffer);
	if (!answer)
	{
		sendClient(data->sending_client.file_client_sock, buffer, strlen(buffer)+1);	// On prévient le client émetteur

		*(data->thread_status) = -1;							// Thread_status indique que le thread attend que l'on lise son code de retour
		pthread_mutex_unlock(data->mutex_thread_status);		// Déverouillage du mutex
		printf("< FTS > Refus du transfert\n\n");
		pthread_exit(NULL);
	}

	printf("< FTS > Le transfert a été accepté par le client destinataire\n");

	sendClient(data->sending_client.file_client_sock, buffer, strlen(buffer)+1);	// On prévient le client émetteur
	recvClient(data->sending_client.file_client_sock, buffer, sizeof(buffer));		// Réception du nombre de paquets et de la taille du dernier paquet
	sendClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));	// Transmission des informations au client destinataire
	
	char_ptr = strchr(buffer, ' ');
	*char_ptr = '\0';
	package_number = atoi(buffer);
	residue_size = atoi(char_ptr + 1);
	recvClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));	// Réception de l'accusé de réception
	sendClient(data->sending_client.file_client_sock, buffer, 1);					// Transmission de l'accusé de réception pour la synchronisation

	for (i = 0; i < package_number; i++)
	{
		if (i == package_number / 4 && package_number >= 500000)					// On affiche le pourcentage seulement si le nombre de paquets est élevé
		{
			printf("< FTS > Le transfert en est à 25%%\n");
		}
		else if (i == package_number / 2 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 50%%\n");
		}
		else if (i == (package_number / 4)*3 && package_number >= 500000)
		{
			printf("< FTS > Le transfert en est à 75%%\n");
		}
		
		recvClient(data->sending_client.file_client_sock, buffer, sizeof(buffer));
		sendClient(data->receiving_client.file_client_sock, buffer, sizeof(buffer));

		/* Accusé de réception (pour la synchronisation) */
		recvClient(data->receiving_client.file_client_sock, buffer, 1);
		sendClient(data->sending_client.file_client_sock, buffer, 1);
	}
	if (residue_size != 0)	// Dans le cas d'un dernier paquet de taille particulière
	{
		recvClient(data->sending_client.file_client_sock, buffer, sizeof(buffer));
		sendClient(data->receiving_client.file_client_sock, buffer, residue_size);

		/* Accusé de réception (pour la synchronisation) */
		recvClient(data->receiving_client.file_client_sock, buffer, 1);
		sendClient(data->sending_client.file_client_sock, buffer, 1);
	}

	*(data->thread_status) = -1;								// Thread_status indique que le thread attend que l'on lise son code de retour
	pthread_mutex_unlock(data->mutex_thread_status);			// Déverouillage du mutex
	printf("< FTS > Le transfert s'est parfaitement déroulé !\n\n");
	pthread_exit(NULL);
}

struct Client newClient(int ssock, int *nb_c, int *max_fd)
{
	/* Cette fonction permet d'ajouter un nouveau client aux tableaux des clients connectés.
	Elle prend en arguments la socket passive du serveur, un pointeur sur le nombre de clients actuel
	et un pointeur sur le descripteur de fichier maximal.
	Elle retourne une variable de type struct Client */

	int msg_client_sock = 0;
	int file_client_sock = 0;
	struct sockaddr_in csin;
	socklen_t sinsize = 0; 		// Variable représentant la taille de csin

	char username[16] = "";		// Création d'un tableau de char pour stocker le nom d'utilisateur

	/* On sait que le nom d'utilisateur ne dépassera pas 16 octets car la vérification a déjà été faite chez le client */

	memset(&csin, 0, sizeof(csin));		// Initialisation de la structure en mettant tous les bits à 0
	sinsize = sizeof(csin);
	msg_client_sock = accept(ssock, (struct sockaddr *)&csin, &sinsize);	// Acceptation de la connexion du client pour les messsages
	if (msg_client_sock == SOCKET_ERROR)
	{
		perror("< FERROR > accept error");
		exit(errno);
	}

	memset(&csin, 0, sizeof(csin));
	sinsize = sizeof(csin);
	file_client_sock = accept(ssock, (struct sockaddr *)&csin, &sinsize);	// Acceptation de la connexion du client pour les messsages
	if (file_client_sock == SOCKET_ERROR)
	{
		perror("< FERROR > accept error");
		exit(errno);
	}

	*nb_c += 1;
	*max_fd = max(max(msg_client_sock, *max_fd), file_client_sock);

	if (recv(msg_client_sock, username, 16, 0) == SOCKET_ERROR)				// Réception du nom d'utilisateur du nouveau client
	{
		perror("< FERROR > recv error");
		exit(errno);
	}

	username[15] = '\0';	// On s'assure que le nom d'utilisateur ne fasse pas plus de 15 caractères sans compter le caractère nul
	struct Client client = {"", 0, 0};
	client.msg_client_sock = msg_client_sock;
	client.file_client_sock = file_client_sock;
	strcpy(client.username, username);

	return client;
}

void rmvClient(struct Client *clients, int i_to_remove, int *nb_c, int *max_fd, int server_sock)
{
	/* Cette fonction permet de déconnecter et supprimer un client du tableau des clients.
	Elle prend en arguments le tableau des clients, l'indice du client à supprimer, un pointeur sur le nombre
	de clients actuellement connectés, un pointeur sur le descripteur de fichier maximal et enfin la socket passive du serveur. */

	int i = 0;

	close(clients[i_to_remove].msg_client_sock);
	close(clients[i_to_remove].file_client_sock);
	for (i = i_to_remove; i < *nb_c-1; i++)
	{
		clients[i] = clients[i+1];	// Réorganisation du tableau
	}

	*nb_c -= 1;
	*max_fd = server_sock;
	for (i = 0; i < *nb_c; i++)
	{
		*max_fd = max(max(clients[i].msg_client_sock, *max_fd), clients[i].file_client_sock);
	}
}

int recvClient(int sock, char *buffer, size_t buffer_size)
{
	/* Cette fonction permet de recevoir un message ou un paquet de la part d'un client et s'occupe des tests.
	Elle prend en paramètres la socket du client en question, un buffer dans lequel stocker le message
	et un entier représentant la taille du buffer en question.
	Elle retourne un booléen qui vaut FALSE seulement si le client s'est déconnecté */

	ssize_t recv_outcome = 0;
	recv_outcome = recv(sock, buffer, buffer_size, 0);
	if (recv_outcome == SOCKET_ERROR)
	{
		perror("< FERROR > recv error");
		exit(errno);
	}
	else if (recv_outcome == 0)
	{
		return FALSE;
	}

	return TRUE;
}

void sendClient(int sock, char *buffer, size_t buffer_size)		// On pourra mettre strlen(buffer)+1 pour le dernier argument pour les chaînes de caractères
{
	/* Cette fonction permet d'envoyer un message ou un paquet à un client déterminé et s'occupe des tests.
	Elle prend en paramètres la socket du client en question, un buffer contenant le message
	et un entier représentant la taille du buffer en question. */

	if (send(sock, buffer, buffer_size, 0) == SOCKET_ERROR)
	{
		perror("< FERROR > send error");
		exit(errno);
	}
}

void sendToAll(struct Client *clients, char *buffer, int clients_nb)
{
	/* Envoie exclusivement un message à tous les clients connectés.
	La fonction prend en arguments le tableau des clients connectés, un buffer contenant le message
	et un entier représentant le nombre de clients connectés simultanément. */

	int i = 0;
	for (i = 0; i < clients_nb; i++)
	{	
		sendClient(clients[i].msg_client_sock, buffer, strlen(buffer)+1);
	}
}

void sendToOther(struct Client *clients, char *buffer, int ind, int clients_nb)
{
	/* Envoie exclusivement un message à tous les clients connectés excepté un.
	La fonction prend en arguments le tableau des clients connectés, un buffer contenant le message,
	l'indice du client non destinataire et un entier représentant le nombre de clients connectés simultanément. */
	
	int i = 0;
	for (i = 0; i < clients_nb; i++)
	{	
		if (i != ind)
		{
			sendClient(clients[i].msg_client_sock, buffer, strlen(buffer)+1);
		}
	}
}

int max(int a, int b)
{
	/* Fonction retournant le maximum entre deux entiers entrés en paramètres */
	return (a >= b) ? a : b;
}
