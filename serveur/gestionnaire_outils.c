#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "../include/commun.h"

Outil outils[MAX_OUTILS];
LogEntry journal[MAX_LOG_ENTRIES];
int journal_index = 0;
pthread_mutex_t journal_verrou = PTHREAD_MUTEX_INITIALIZER;
int current_strategy = STRATEGIE_WAIT_DIE;
void initialize_server();
void cleanup_server();
void handle_signal(int sig);
void ajouter_log(int bras_id, const char *action, ...) {
    va_list args;
    va_start(args, action);
    pthread_mutex_lock(&journal_verrou);
    if (journal_index < MAX_LOG_ENTRIES) {
        time(&journal[journal_index].timestamp);
        journal[journal_index].bras_id = bras_id;
        char formatted_msg[100];
        vsnprintf(formatted_msg, sizeof(formatted_msg), action, args);
        strncpy(journal[journal_index].action, formatted_msg, 49);
        journal[journal_index].action[49] = '\0';
        journal_index++;
        printf("[LOG] bras %d: %s\n", bras_id, formatted_msg);
    }
    pthread_mutex_unlock(&journal_verrou);
    va_end(args);
}
int verrouiller_avec_timeout(pthread_mutex_t *mutex, int timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += timeout_ms * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec %= 1000000000;
    }
    return pthread_mutex_timedlock(mutex, &ts);
}
int appliquer_wait_die(int bras_id, int outil_id) {
    if (outils[outil_id].bras_proprietaire != -1 && 
        bras_id > outils[outil_id].bras_proprietaire) {
        return 0;
    }
    return 1; 
}
int appliquer_wound_wait(int bras_id, int outil_id) {
    if (outils[outil_id].bras_proprietaire != -1 && 
        bras_id < outils[outil_id].bras_proprietaire) {
        pthread_mutex_unlock(&outils[outil_id].verrou);
        outils[outil_id].disponible = 1;
        outils[outil_id].bras_proprietaire = -1;
        ajouter_log(outils[outil_id].bras_proprietaire, "outil %d libere (wound)", outil_id);
        return 0;
    }
    return 1;
}

int acquerir_outils(int bras_id, int id1, int id2, int client_socket) {
    int first = id1 < id2 ? id1 : id2;
    int second = id1 > id2 ? id1 : id2;
    char log_msg[100];
    if (verrouiller_avec_timeout(&outils[first].verrou, TIMEOUT_MS) != 0) {
        snprintf(log_msg, sizeof(log_msg), "timeout outil %d", first);
        ajouter_log(bras_id, log_msg);
        send(client_socket, "timeout\n", 8, 0);
        return 0;
    }
    if (verrouiller_avec_timeout(&outils[second].verrou, TIMEOUT_MS) != 0) {
        pthread_mutex_unlock(&outils[first].verrou);
        snprintf(log_msg, sizeof(log_msg), "timeout outil %d", second);
        ajouter_log(bras_id, log_msg);
        send(client_socket, "timeout\n", 8, 0);
        return 0;
    }
    int success = 1;
    if (current_strategy == STRATEGIE_WAIT_DIE) {
        success = appliquer_wait_die(bras_id, first) && 
                 appliquer_wait_die(bras_id, second);
    } else if (current_strategy == STRATEGIE_WOUND_WAIT) {
        success = appliquer_wound_wait(bras_id, first) && 
                 appliquer_wound_wait(bras_id, second);
    }
    if (!success) {
        pthread_mutex_unlock(&outils[first].verrou);
        pthread_mutex_unlock(&outils[second].verrou);
        snprintf(log_msg, sizeof(log_msg), "rejete (strategie %s)", 
                current_strategy == STRATEGIE_WAIT_DIE ? "wait-die" : "wound-wait");
        ajouter_log(bras_id, log_msg);
        send(client_socket, "rejete\n", 7, 0);
        return 0;
    }
    outils[first].disponible = 0;
    outils[second].disponible = 0;
    outils[first].bras_proprietaire = bras_id;
    outils[second].bras_proprietaire = bras_id;
    snprintf(log_msg, sizeof(log_msg), "outils %d et %d acquis", id1, id2);
    ajouter_log(bras_id, log_msg);
    send(client_socket, "ok\n", 3, 0);
    return 1;
}
void liberer_outil(int bras_id, int outil_id, int client_socket) {
    pthread_mutex_lock(&outils[outil_id].verrou);
    outils[outil_id].disponible = 1;
    outils[outil_id].bras_proprietaire = -1;
    pthread_mutex_unlock(&outils[outil_id].verrou);
    ajouter_log(bras_id, "outil %d libere", outil_id);
    send(client_socket, "ok\n", 3, 0);
}
void changer_strategie(int bras_id, const char *strategy, int client_socket) {
    if (strcmp(strategy, "wait-die") == 0) {
        current_strategy = STRATEGIE_WAIT_DIE;
        ajouter_log(bras_id, "strategie changee en wait-die");
        send(client_socket, "strategie wait-die\n", 19, 0);
    } else if (strcmp(strategy, "wound-wait") == 0) {
        current_strategy = STRATEGIE_WOUND_WAIT;
        ajouter_log(bras_id, "strategie changee en wound-wait");
        send(client_socket, "strategie wound-wait\n", 20, 0);
    } else {
        ajouter_log(bras_id, "strategie inconnue: %s", strategy);
        send(client_socket, "erreur: strategie inconnue\n", 26, 0);
    }
}
void *gerer_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bras_id = -1;
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) > 0) {
        if (sscanf(buffer, "id %d", &bras_id) == 1) {
            ajouter_log(bras_id, "connecte");
        }
    }
    while (recv(client_socket, buffer, BUFFER_SIZE, 0) > 0) {
        printf("Recu: %s\n", buffer);
        int id1, id2;
        char command[20], strategy[20];
        if (sscanf(buffer, "demande_2_outils %d %d", &id1, &id2) == 2) {
            acquerir_outils(bras_id, id1, id2, client_socket);
        } 
        else if (sscanf(buffer, "liberation_outil %d", &id1) == 1) {
            liberer_outil(bras_id, id1, client_socket);
        } 
        else if (sscanf(buffer, "set_strategy %s", strategy) == 1) {
            changer_strategie(bras_id, strategy, client_socket);
        } 
        else if (strncmp(buffer, "sync", 4) == 0) {
            send(client_socket, "sync_ok\n", 8, 0);
            ajouter_log(bras_id, "synchronisation");
        } 
        else {
            ajouter_log(bras_id, "commande inconnue: %s", buffer);
            send(client_socket, "erreur: commande inconnue\n", 26, 0);
        }
        memset(buffer, 0, BUFFER_SIZE);
    }
    close(client_socket);
    ajouter_log(bras_id, "deconnecte");
    return NULL;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("serveur lance sur le port %d...\n", PORT);
    for (int i = 0; i < MAX_OUTILS; i++) {
        outils[i].id = i;
        outils[i].disponible = 1;
        outils[i].bras_proprietaire = -1;
        pthread_mutex_init(&outils[i].verrou, NULL);
    }
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_socket = malloc(sizeof(int));   
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (*client_socket < 0) {
            perror("accept failed");
            free(client_socket);
            continue;
        }
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, gerer_client, client_socket) != 0) {
            perror("thread creation failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }
        pthread_detach(thread_id);
    }
    close(server_socket);
    return 0;
}