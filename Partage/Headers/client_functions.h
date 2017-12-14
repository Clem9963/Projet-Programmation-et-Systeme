#ifndef CLIENT_FUNCTIONS_H_INCLUDED
#define CLIENT_FUNCTIONS_H_INCLUDED

/* Fonction relative au répertoire */
void getDirectory(char *path, size_t length);

/* Fonction d'initialisation */
int connectSocket(char* address, int port);

/* Fonctions liées à l'envoi et à la réception */
int recvServer(int sock, char *buffer, size_t buffer_size);
int sendServer(int sock, char *buffer);

#endif