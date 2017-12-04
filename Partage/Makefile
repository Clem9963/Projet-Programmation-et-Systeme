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

# Règles
all : Object_files/main.o Object_files/fonctions.o
	$(COMPILER) $(LFLAGS) $(CFLAGS) Object_files/main.o Object_files/fonctions.o -o executable.bin
	@echo
	@echo Exécutable généré via $(COMPILER) en mode $(BUILD) $(LIB)

Object_files/main.o : main.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c main.c -o Object_files/main.o
 
Object_files/fonctions.o : Sources/fonctions.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c Sources/fonctions.c -o Object_files/fonctions.o

# Suppression des fichiers objets

clean:
	rm -rf *.o

# Suppression de tous les fichiers, sauf les sources, en vue d'une reconstruction complète

mrproper: clean
	rm -rf executable