#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "server_net.h"
#include "protocol.h"
#include "protocol_io.h"

/*
    SIMPLE SERVER STATE

    - client_fd: aktualne pripojeny klient (len 1 klient)
    - sim_running: ci bezi simulacia
    - sim_params: parametre simulacie (z MSG_START_SIM)
*/
static int g_running = 1;

static int g_client_fd = -1;
static pthread_mutex_t g_client_lock = PTHREAD_MUTEX_INITIALIZER;

static int g_sim_running = 0;
static start_sim_t g_sim_params; // ulozime sem START_SIM (v network order alebo host order - my si dame host)
static pthread_mutex_t g_sim_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_sim_cv = PTHREAD_COND_INITIALIZER;

/*
    Helper: posli spravu klientovi (ak je pripojeny)
*/
static void send_to_client(uint32_t type, const void* payload, uint32_t payload_len)
{
    pthread_mutex_lock(&g_client_lock);
    int fd = g_client_fd;
    pthread_mutex_unlock(&g_client_lock);

    if (fd < 0) return;

    // ak send zlyha, zatial len ignorujeme (neskor mozeme odpojit klienta)
    protocol_send(fd, type, payload, payload_len);
}

/*
    Sim thread:
    - caka, kym pride START_SIM
    - potom spravi "replications" cyklus
    - po kazdej replikacii posle PROGRESS
    - na konci posle STATUS + SUMMARY_DONE (zatial bez realnych dat)
*/
static void* simulation_thread(void* arg)
{
    (void)arg;

    while (g_running) {

        // pocka na START_SIM
        pthread_mutex_lock(&g_sim_lock);

        while (g_running && g_sim_running == 0) {
            pthread_cond_wait(&g_sim_cv, &g_sim_lock);
        }

        if (!g_running) {
            pthread_mutex_unlock(&g_sim_lock);
            break;
        }

        // vyberieme parametre do lokalnych premennych (host order)
        uint32_t reps = g_sim_params.replications;
        pthread_mutex_unlock(&g_sim_lock);

        // simulacia "bezi"
        const char* started = "simulation running";
        send_to_client(MSG_STATUS, started, (uint32_t)strlen(started));

        for (uint32_t i = 1; i <= reps; i++) {

            // tu neskor pride realna logika random walk
            // zatial len simulujeme pracu
            usleep(300 * 1000); // 300 ms

            progress_t p;
            p.repl_index = htonl(i);
            p.repl_total = htonl(reps);
            send_to_client(MSG_PROGRESS, &p, sizeof(p));

            // ak niekto poslal STOP_SIM, tak skonci cyklus
            pthread_mutex_lock(&g_sim_lock);
            int still_running = g_sim_running;
            pthread_mutex_unlock(&g_sim_lock);

            if (!still_running) {
                const char* stopped = "simulation stopped";
                send_to_client(MSG_STATUS, stopped, (uint32_t)strlen(stopped));
                break;
            }
        }

        // 3) koniec simulacie
        send_to_client(MSG_SUMMARY_DONE, NULL, 0);

        const char* done = "simulation finished";
        send_to_client(MSG_STATUS, done, (uint32_t)strlen(done));

        pthread_mutex_lock(&g_sim_lock);
        g_sim_running = 0;
        pthread_mutex_unlock(&g_sim_lock);
    }

    return NULL;
}

static int read_port(int argc, char** argv)
{
    if (argc < 2) return 5555;

    int p = atoi(argv[1]);
    if (p <= 0 || p > 65535) {
        printf("[server] invalid port, using 5555\n");
        return 5555;
    }
    return p;
}

int main(int argc, char** argv)
{
    int port = read_port(argc, argv);

    int listen_fd = network_listen_on_port(port, 8);
    if (listen_fd < 0) {
        printf("[server] failed to start server\n");
        return 1;
    }

    printf("[server] running on port %d\n", port);

    pthread_t sim_thread;
    if (pthread_create(&sim_thread, NULL, simulation_thread, NULL) != 0) {
        printf("[server] failed to create simulation thread\n");
        network_close_fd(&listen_fd);
        return 1;
    }

    while (g_running) {
        printf("[server] waiting for client...\n");

        int client_fd = network_accept_client(listen_fd);
        if (client_fd < 0) {
            printf("[server] accept failed\n");
            continue;
        }

        pthread_mutex_lock(&g_client_lock);
        // ak bol stary klient, zavrieme ho (simple pravidlo: len 1 klient)
        if (g_client_fd >= 0) {
            network_close_fd(&g_client_fd);
        }
        g_client_fd = client_fd;
        pthread_mutex_unlock(&g_client_lock);

        printf("[server] client connected\n");

        // command loop
        while (1) {
            msg_header_t hdr;
            void* payload = NULL;

            int r = protocol_receive(client_fd, &hdr, &payload);

            if (r == 0) {
                printf("[server] client disconnected\n");
                break;
            }
            if (r < 0) {
                printf("[server] recv error\n");
                break;
            }

            switch (hdr.type) {

                case MSG_HELLO: {
                    hello_t* h = (hello_t*)payload;
                    uint32_t version = ntohl(h->version);
                    printf("[server] HELLO version=%u\n", version);

                    protocol_send(client_fd, MSG_WELCOME, NULL, 0);
                    break;
                }

                case MSG_START_SIM: {
                    start_sim_t* s = (start_sim_t*)payload;

                    // konverzia do host order a ulozenie
                    start_sim_t host;
                    host.world_w = ntohl(s->world_w);
                    host.world_h = ntohl(s->world_h);
                    host.replications = ntohl(s->replications);
                    host.K = ntohl(s->K);

                    host.p_up = ntohl(s->p_up);
                    host.p_down = ntohl(s->p_down);
                    host.p_left = ntohl(s->p_left);
                    host.p_right = ntohl(s->p_right);

                    host.mode = ntohl(s->mode);
                    host.view = ntohl(s->view);

                    printf("[server] START_SIM w=%u h=%u reps=%u K=%u\n",
                           host.world_w, host.world_h, host.replications, host.K);

                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_params = host;
                    g_sim_running = 1;
                    pthread_cond_signal(&g_sim_cv);
                    pthread_mutex_unlock(&g_sim_lock);

                    const char* msg = "start accepted";
                    protocol_send(client_fd, MSG_STATUS, msg, (uint32_t)strlen(msg));
                    break;
                }

                case MSG_SET_MODE: {
                    set_mode_t* m = (set_mode_t*)payload;
                    uint32_t mode = ntohl(m->mode);
                    printf("[server] SET_MODE %u\n", mode);

                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_params.mode = mode;
                    pthread_mutex_unlock(&g_sim_lock);

                    const char* msg = "mode changed";
                    protocol_send(client_fd, MSG_STATUS, msg, (uint32_t)strlen(msg));
                    break;
                }

                case MSG_SET_VIEW: {
                    set_view_t* v = (set_view_t*)payload;
                    uint32_t view = ntohl(v->view);
                    printf("[server] SET_VIEW %u\n", view);

                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_params.view = view;
                    pthread_mutex_unlock(&g_sim_lock);

                    const char* msg = "view changed";
                    protocol_send(client_fd, MSG_STATUS, msg, (uint32_t)strlen(msg));
                    break;
                }

                case MSG_STOP_SIM: {
                    printf("[server] STOP_SIM\n");

                    pthread_mutex_lock(&g_sim_lock);
                    g_sim_running = 0;
                    pthread_mutex_unlock(&g_sim_lock);

                    const char* msg = "stop accepted";
                    protocol_send(client_fd, MSG_STATUS, msg, (uint32_t)strlen(msg));
                    break;
                }

                case MSG_BYE: {
                    printf("[server] BYE\n");
                    free(payload);
                    payload = NULL;
                    goto disconnect_client;
                }

                default:
                    printf("[server] unknown msg type=%u\n", hdr.type);
                    break;
            }

            free(payload);
        }

disconnect_client:
        // odpoj klienta
        pthread_mutex_lock(&g_client_lock);
        if (g_client_fd == client_fd) {
            network_close_fd(&g_client_fd);
        }
        pthread_mutex_unlock(&g_client_lock);

        network_close_fd(&client_fd);
    }

    g_running = 0;

    // zobud sim thread aby sa vedel ukoncit
    pthread_mutex_lock(&g_sim_lock);
    pthread_cond_signal(&g_sim_cv);
    pthread_mutex_unlock(&g_sim_lock);

    pthread_join(sim_thread, NULL);
    network_close_fd(&listen_fd);

    printf("[server] stopped\n");
    return 0;
}
