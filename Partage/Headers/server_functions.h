#ifndef SERVER_FUNCTIONS_H_INCLUDED
#define SERVER_FUNCTIONS_H_INCLUDED

#define h_addr h_addr_list[0]
#define SOCKET_ERROR -1			/* code d'erreur des sockets  */

struct Client
{
	char username[16];
	int msg_client_sock;
	int file_client_sock;
};

struct TransferDetails
{
	struct Client sending_client;
	struct Client receiving_client;
	char *request;
	int *thread_status;
	pthread_mutex_t *mutex_thread_status;
};

/* Fonction d'initialisation*/
int listenSocket(int port);

/* Fonctions relatives à la requête /sendto */
int getClient(struct Client *clients, int clients_nb, char *buffer);
void *transferControl(void *src_data);

/* Gestion des clients */
struct Client newClient(int ssock, int *nb_c, int *max_fd);
void rmvClient(struct Client *clients, int i_to_remove, int *nb_c, int *max_fd, int server_sock);

/* Envoi et réception */
int recvClient(int sock, char *buffer, size_t buffer_size);
int sendClient(int sock, char *buffer, size_t buffer_size);

/* Fonction annexe */
int max(int a, int b);

#endif