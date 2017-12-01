# Choix des arguments pour le compilateur

COMPILER = gcc
BUILD = debug
LIB = shared

ifeq ($(DEBUG),debug)
CFLAGS = -Wall -g
else
CFLAGS = -Wall
endif

ifeq ($(LIB),static)
LFLAGS = -static
endif

# Règles
all : main.o fonctions.o
	$(COMPILER) $(LFLAGS) $(CFLAGS) main.o fonctions.o -o executable.bin
	@echo
	@echo Exécutable généré via $(COMPILER) en mode $(BUILD) $(LIB)

main.o : main.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c main.c -o main.o
 
fonctions.o : Sources/fonctions.c
	$(COMPILER) $(LFLAGS) $(CFLAGS) -I Headers -c Sources/fonctions.c -o fonctions.o

# Suppression des fichiers objets

clean:
	rm -rf *.o

# Suppression de tous les fichiers, sauf les sources, en vue d'une reconstruction complète

mrproper: clean
	rm -rf executable