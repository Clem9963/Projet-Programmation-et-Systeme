#ifndef CLIENT_FUNCTIONS_H_INCLUDED
#define CLIENT_FUNCTIONS_H_INCLUDED

#define h_addr h_addr_list[0]
#define SOCKET_ERROR -1			/* code d'erreur des sockets  */

struct TransferDetails
{
	char *path;
	int file_server_sock;
	int *thread_status;
	pthread_mutex_t *mutex_thread_status;
};

/* Fonction relatives à la requête /sendto */
int verifyDirectory(char *path);
int verifySendingRequest(char *buffer, char *dest_username, char *path);
void *transferSendControl(void *src_data);

/* Fonction d'initialisation */
void connectSocket(char* address, int port, int *msg_server_sock, int *file_server_sock);

/* Fonctions liées à l'envoi et à la réception (bas niveau) */
int recvServer(int sock, char *buffer, size_t buffer_size);
int sendServer(int sock, char *buffer);

#endif
