#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>

#define TAILLE_BUF 1000
#define TAILLE_PSEUDO 15
#define PORT 54888

void demandeIP(char *adresseIP);
void demandePseudo(char *pseudo);
int connect_socket(char *adresse, int port);
void read_serveur(int sock, char *buffer, char conversation[LINES - 6][TAILLE_BUF], int *ligne, WINDOW *fenHaut, WINDOW *fenBas);
void write_serveur(int sock, char *buffer);
void ecritDansConv(char *buffer, char conversation[LINES - 6][TAILLE_BUF], int *ligne, WINDOW *fenHaut, WINDOW *fenBas);
void concatener(char *buffer, char *pseudo);
void initInterface(WINDOW *fenHaut, WINDOW *fenBas);
void rafraichit(WINDOW *fenHaut, WINDOW *fenBas);
void effaceMemoire(int sock, WINDOW *fenHaut, WINDOW *fenBas);