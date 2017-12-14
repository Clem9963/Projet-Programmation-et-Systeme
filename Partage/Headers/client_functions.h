#ifndef CLIENT_FUNCTIONS_H_INCLUDED
#define CLIENT_FUNCTIONS_H_INCLUDED

#define h_addr h_addr_list[0]
#define SOCKET_ERROR -1			/* code d'erreur des sockets  */

/* Fonction relatives à la requête /sendto */
int verifyDirectory(char *path);
int verifySendingRequest(char *buffer, char *dest_username, char *path);

/* Fonction d'initialisation */
void connectSocket(char* address, int port, int *msg_server_sock, int *file_server_sock);

/* Fonctions liées à l'envoi et à la réception (bas niveau) */
int recvServer(int sock, char *buffer, size_t buffer_size);
int sendServer(int sock, char *buffer);

#endif
