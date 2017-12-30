#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define FORMATTING_BUFFER_SIZE 1048
#define USERNAME_SIZE 16

#endif
