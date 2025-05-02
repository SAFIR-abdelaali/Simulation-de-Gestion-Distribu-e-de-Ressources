all: bras_robotique gestionnaire_outils
bras_robotique: bras_robotique.o
	gcc bras_robotique.o -lpthread -o bras_robotique
gestionnaire_outils: gestionnaire_outils.o
	gcc gestionnaire_outils.o -lpthread -o gestionnaire_outils
bras_robotique.o: client/bras_robotique.c include/commun.h
	gcc -c client/bras_robotique.c -Iinclude -o bras_robotique.o
gestionnaire_outils.o: serveur/gestionnaire_outils.c include/commun.h
	gcc -c serveur/gestionnaire_outils.c -Iinclude -o gestionnaire_outils.o
clean:
	rm -f *.o bras_robotique gestionnaire_outils
