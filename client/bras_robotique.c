#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include "../include/commun.h"

int sock;
int bras_id;
int outil1_id = 1;  // Valeurs par défaut
int outil2_id = 2;

void *thread_idle(void *arg) {
    while (1) {
        printf("[idle] Bras %d en attente...\n", bras_id);
        sleep(3 + rand() % 3);  // Attente aléatoire entre 3-5 secondes
    }
    return NULL;
}

int demander_outil(int outil_id) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "demande_outil %d\n", outil_id);
    send(sock, msg, strlen(msg), 0);
    
    char response[BUFFER_SIZE] = {0};
    recv(sock, response, sizeof(response), 0);
    
    if (strncmp(response, "ok", 2) == 0) {
        printf("[comm] Bras %d: outil %d reçu.\n", bras_id, outil_id);
        return 1;
    }
    return 0;
}

int demander_deux_outils(int id1, int id2) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "demande_2_outils %d %d\n", id1, id2);
    send(sock, msg, strlen(msg), 0);
    
    char response[BUFFER_SIZE] = {0};
    recv(sock, response, sizeof(response), 0);
    
    if (strncmp(response, "ok", 2) == 0) {
        printf("[comm] Bras %d: outils %d et %d reçus.\n", bras_id, id1, id2);
        return 1;
    }
    return 0;
}

void liberer_outil(int outil_id) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "liberation_outil %d\n", outil_id);
    send(sock, msg, strlen(msg), 0);
    printf("[assemblage] Bras %d: outil %d libéré.\n", bras_id, outil_id);
}

void *thread_comm(void *arg) {
    // Envoi de l'identification
    char id_msg[BUFFER_SIZE];
    snprintf(id_msg, sizeof(id_msg), "id %d\n", bras_id);
    send(sock, id_msg, strlen(id_msg), 0);

    while (1) {
        // Demande des outils (avec plusieurs stratégies possibles)
        int succes = 0;
        int tentative = 0;
        
        while (!succes && tentative < 3) {
            // Stratégie 1: Demander les deux outils ensemble
            succes = demander_deux_outils(outil1_id, outil2_id);
            
            if (!succes) {
                // Stratégie 2: Demander les outils un par un
                succes = demander_outil(outil1_id) && demander_outil(outil2_id);
            }
            
            if (!succes) {
                printf("[comm] Bras %d: échec tentative %d, réessai...\n", bras_id, tentative+1);
                sleep(1);
                tentative++;
            }
        }

        if (succes) {
            // Synchronisation avec le serveur
            char sync_msg[BUFFER_SIZE] = "sync\n";
            send(sock, sync_msg, strlen(sync_msg), 0);
            
            char sync_response[BUFFER_SIZE] = {0};
            recv(sock, sync_response, sizeof(sync_response), 0);
            
            if (strncmp(sync_response, "sync_ok", 7) == 0) {
                printf("[comm] Bras %d: synchronisation réussie\n", bras_id);
            }
            
            sleep(1); // Pause avant libération
        } else {
            printf("[comm] Bras %d: abandon après 3 tentatives\n", bras_id);
            sleep(5); // Attente plus longue en cas d'échec
        }
    }
    return NULL;
}

void *thread_assemble(void *arg) {
    while (1) {
        // Simule l'assemblage seulement si les outils sont acquis
        printf("[assemblage] Bras %d: tâche en cours avec outils %d et %d...\n", 
              bras_id, outil1_id, outil2_id);
        sleep(5 + rand() % 5);  // Durée aléatoire entre 5-9 secondes
        
        // Libération des outils
        liberer_outil(outil1_id);
        liberer_outil(outil2_id);
        
        sleep(1); // Pause entre les cycles
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <bras_id> [outil1] [outil2]\n", argv[0]);
        return 1;
    }
    
    bras_id = atoi(argv[1]);
    
    // Permet de spécifier les outils en paramètre
    if (argc >= 4) {
        outil1_id = atoi(argv[2]);
        outil2_id = atoi(argv[3]);
    }
    
    // Configuration du socket
    struct sockaddr_in serv_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP_SERVEUR, &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    
    printf("Bras %d connecté au serveur (outils %d et %d).\n", 
          bras_id, outil1_id, outil2_id);
    
    // Initialisation des threads
    pthread_t idle, comm, assemble;
    pthread_create(&idle, NULL, thread_idle, NULL);
    pthread_create(&comm, NULL, thread_comm, NULL);
    pthread_create(&assemble, NULL, thread_assemble, NULL);
    
    pthread_join(idle, NULL);
    pthread_join(comm, NULL);
    pthread_join(assemble, NULL);
    
    close(sock);
    return 0;
}