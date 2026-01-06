#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <stdatomic.h>

typedef struct {
    int port;

    int listen_fd;
    int client_fd; // -1 ak nikto nie je pripojený

    pthread_t sim_thread;
    pthread_mutex_t client_lock;

    atomic_int running; // 1 = beží, 0 = končí
} server_ctx_t;

#endif
