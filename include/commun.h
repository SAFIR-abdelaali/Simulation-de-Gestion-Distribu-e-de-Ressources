#ifndef COMMUN_H
#define COMMUN_H
#include <pthread.h>
#include <time.h>
#define PORT 12345
#define MAX_OUTILS 5
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_LOG_ENTRIES 1000
#define IP_SERVEUR "127.0.0.1"
#define TIMEOUT_MS 5000 
#define STRATEGIE_WAIT_DIE    1
#define STRATEGIE_WOUND_WAIT  2
#define STRATEGIE_DEFAUT  STRATEGIE_WAIT_DIE

typedef struct {
    int id;
    int disponible;
    pthread_mutex_t verrou;
    int bras_proprietaire;
} Outil;

typedef struct {
    time_t timestamp;
    int bras_id;
    char action[50];
} LogEntry;

#endif