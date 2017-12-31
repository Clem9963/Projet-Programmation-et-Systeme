#ifndef CLIENT_FUNCTIONS_H_INCLUDED
#define CLIENT_FUNCTIONS_H_INCLUDED

#define h_addr h_addr_list[0]
#define SOCKET_ERROR -1			// Code d'erreur des sockets 

struct TransferDetails
{
	char *path;
	int file_server_sock;
	int msg_server_sock;
	int *thread_status;
	pthread_mutex_t *mutex_thread_status;
	char **conversation;
	int *line;
	WINDOW *top_win;
	WINDOW *bottom_win;
};

/* Fonction relatives à la requête /sendto */
int verifyDirectory(char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win);
int answerSendingRequest(char *request, char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win);
int verifySendingRequest(char *buffer, char *dest_username, char *path, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win);
void *transferSendControl(void *src_data);
void *transferRecvControl(void *src_data);

/* Fonction d'initialisation */
void connectSocket(char* address, int port, int *msg_server_sock, int *file_server_sock);
void initInterface(WINDOW *top_win, WINDOW *bottom_win);

/* Fonctions d'affichage */
void writeInConv(char *buffer, char **conversation, int *line, WINDOW *top_win, WINDOW *bottom_win);
void convRefresh(WINDOW *top_win, WINDOW *bottom_win);

/* Fonctions liées à l'envoi et à la réception (bas niveau) */
int recvServer(int sock, char *buffer, size_t buffer_size);
void sendServer(int sock, char *buffer, size_t buffer_size);

/* Fonction pour effacer les différents élements en mémoire */
void clearMemory(int msg_server_sock, int file_server_sock, char **conversation, WINDOW *top_win, WINDOW *bottom_win);

#endif
