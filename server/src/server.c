#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // close(), usleep()
#include <pthread.h>
#include <sys/socket.h>
#include "server_net.h"

// globalna premenna, podla ktorej vieme, ci server este bezi
// pouziva ju hlavne hlavne vlakno a simulacne vlakno
static int g_running = 1;

/*
 * Toto je pomocne vlakno servera.
 * Zatial nerobi realnu simulaciu nahodnej pochodzky.
 * Len kazdych 500 ms spravi "tick".
 *
 * Neskor sem pride:
 *  - vypocet replikacii
 *  - pohyb chodca
 *  - pocitanie krokov / pravdepodobnosti
 */
static void* simulation_thread(void* arg)
{
    // argument zatial nepotrebujeme
    (void)arg;

    int tick = 0;

    // vlakno bezi, pokial server bezi
    while (g_running) {
        // uspime vlakno na 500 ms
        usleep(500 * 1000);

        tick++;

        // sem neskor dam jeden krok simulacie
        // napr. spracovanie jednej replikacie
        // printf("[server] sim tick %d\n", tick);

        (void)tick; // aby kompilator nepisal warning
    }

    return NULL;
}

/*
 * Nacitanie portu z argumentov prikazoveho riadku
 * spustenie servera:
 *   ./server 5555
 *
 * ak port nie je zadany alebo je zly, pouzije sa 5555
 */
static int read_port(int argc, char** argv)
{
    // ak nebol zadany port
    if (argc < 2) {
        return 5555;
    }

    int p = atoi(argv[1]);

    // kontrola rozsahu portu
    if (p <= 0 || p > 65535) {
        printf("[server] zly port, pouzijem 5555\n");
        return 5555;
    }

    return p;
}

int main(int argc, char** argv)
{
    // nacitanie portu
    int port = read_port(argc, argv);

    // 1) vytvorenie server socketu a listen na porte
    int listen_fd = net_listen_on_port(port, 8);
    if (listen_fd < 0) {
        printf("[server] nepodarilo sa spustit server na porte %d\n", port);
        return 1;
    }

    printf("[server] bezi na porte %d\n", port);

    // 2) spustenie pomocneho vlakna (simulacia)
    pthread_t sim_thread;
    if (pthread_create(&sim_thread, NULL, simulation_thread, NULL) != 0) {
        printf("[server] nepodarilo sa spustit vlakno\n");
        net_close_fd(&listen_fd);
        return 1;
    }

    // 3) hlavny cyklus servera
    // server stale caka na noveho klienta
    while (g_running) {
        printf("[server] cakam na klienta...\n");

        // cakanie na pripojenie klienta
        int client_fd = net_accept_client(listen_fd);
        if (client_fd < 0) {
            printf("[server] accept zlyhal\n");
            continue;
        }

        printf("[server] klient pripojeny\n");

        /*
         * 4) jednoduchy textovy protokol:
         *
         * klient -> server:
         *   "PING" -> server odpovie "PONG"
         *   "QUIT" -> server odpoji klienta
         *   "EXIT" -> server sa vypne
         */
        char buf[256];

        while (1) {
            // vynulujeme buffer
            memset(buf, 0, sizeof(buf));

            // prijemca caka na spravu od klienta
            ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);

            if (n < 0) {
                printf("[server] prijemca chyba\n");
                break;
            }

            if (n == 0) {
                // klient sa korektne odpojil
                printf("[server] klient sa odpojil\n");
                break;
            }

            // odstranenie \n alebo \r zo spravy
            for (int i = 0; buf[i] != '\0'; i++) {
                if (buf[i] == '\n' || buf[i] == '\r') {
                    buf[i] = '\0';
                    break;
                }
            }

            printf("[server] prislo: '%s'\n", buf);


            if (strcmp(buf, "QUIT") == 0) {
                const char* resp = "Maj sa.\n";
                send(client_fd, resp, strlen(resp), 0);
                break; // koniec komunikacie s klientom
            }
            else if (strcmp(buf, "EXIT") == 0) {
                const char* resp = "Server sa zastavuje.\n";
                send(client_fd, resp, strlen(resp), 0);
                g_running = 0; // zastavi server aj simulacne vlakno
                break;
            }
            else {
                const char* resp = "Nespravny prikaz.\n";
                send(client_fd, resp, strlen(resp), 0);
            }
        }

        // zatvorenie socketu klienta
        net_close_fd(&client_fd);
    }

    // ukoncenie servera
    g_running = 0;
    pthread_join(sim_thread, NULL);
    net_close_fd(&listen_fd);

    printf("[server] koniec\n");
    return 0;
}
