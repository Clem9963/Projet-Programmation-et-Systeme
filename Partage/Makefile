# Choix des arguments pour le compilateur

COMPILER=gcc
BUILD=debug
LIB=shared

ifeq ($(BUILD),debug)
	CFLAGS=-Wall -Wextra -g
endif

ifeq ($(LIB),static)
	LFLAGS=-static
endif

# Règles Linkage

all : server client

server : Object_files/server.o Object_files/server_functions.o
	$(COMPILER) $(LFLAGS) $(CFLAGS) Object_files/server.o Object_files/server_functions.o -o server.bin -lpthread
	@echo
	@echo Exécutable généré via $(COMPILER) en mode $(BUILD) $(LIB)

client : Object_files/client.o Object_files/client_functions.o
	$(COMPILER) $(LFLAGS) $(CFLAGS) Object_files/client.o Object_files/client_functions.o -o client.bin -lpthread
	@echo
	@echo Exécutable généré via $(COMPILER) en mode $(BUILD) $(LIB)


# Règles Compilation

Object_files/server.o : server.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c server.c -o Object_files/server.o

Object_files/client.o : client.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c client.c -o Object_files/client.o
 
Object_files/server_functions.o : Sources/server_functions.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c Sources/server_functions.c -o Object_files/server_functions.o

Object_files/client_functions.o : Sources/client_functions.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c Sources/client_functions.c -o Object_files/client_functions.o


# Suppression des fichiers objets

clean:
	rm -rf Object_files/*.o

# Suppression de tous les fichiers, sauf les sources, en vue d'une reconstruction complète

mrproper: clean
	rm -rf executable