#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "server_net.h"
#include "protocol.h"
#include "protocol_io.h"
#include "../server/headers/simulation.h"

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

        // pockaj na START_SIM
        pthread_mutex_lock(&g_sim_lock);
        while (g_running && g_sim_running == 0) {
            pthread_cond_wait(&g_sim_cv, &g_sim_lock);
        }

        if (!g_running) {
            pthread_mutex_unlock(&g_sim_lock);
            break;
        }

        // skopiruj parametre do lokalnych premennych (host order)
        uint32_t w    = g_sim_params.world_w;
        uint32_t h    = g_sim_params.world_h;
        uint32_t reps = g_sim_params.replications;
        uint32_t K    = g_sim_params.K;

        move_probs_t probs;
        probs.p_up    = g_sim_params.p_up;
        probs.p_down  = g_sim_params.p_down;
        probs.p_left  = g_sim_params.p_left;
        probs.p_right = g_sim_params.p_right;

        uint32_t view = g_sim_params.view; // 0=avg, 1=prob
        pthread_mutex_unlock(&g_sim_lock);

        // status len raz
        send_to_client(MSG_STATUS, "simulation running (summary)",(uint32_t)strlen("simulation running (summary)"));

        int center_x = (int)(w / 2);
        int center_y = (int)(h / 2);

        size_t n = (size_t)w * (size_t)h;

        uint64_t* total_steps = (uint64_t*)calloc(n, sizeof(uint64_t));
        uint32_t* success_k   = (uint32_t*)calloc(n, sizeof(uint32_t));

        if (!total_steps || !success_k) {
            send_to_client(MSG_STATUS, "malloc failed", (uint32_t)strlen("malloc failed"));
            free(total_steps);
            free(success_k);

            pthread_mutex_lock(&g_sim_lock);
            g_sim_running = 0;
            pthread_mutex_unlock(&g_sim_lock);
            continue;
        }

        int max_steps = (int)(w * h * 50);
        if (max_steps < 1000) max_steps = 1000;

        // vykonaj presne reps replikacii (len raz)
        for (uint32_t r = 1; r <= reps; r++) {

            // ak niekto dal STOP_SIM, skonci
            pthread_mutex_lock(&g_sim_lock);
            int still_running = g_sim_running;
            pthread_mutex_unlock(&g_sim_lock);
            if (!still_running) break;

            // prejdime vsetky bunky sveta
            for (uint32_t yy = 0; yy < h; yy++) {
                for (uint32_t xx = 0; xx < w; xx++) {

                    size_t idx = (size_t)yy * (size_t)w + (size_t)xx;

                    int steps = simulate_walking((int)xx, (int)yy, (int)w, (int)h, center_x, center_y, &probs,max_steps);

                    total_steps[idx] += (uint64_t)steps;

                    if ((uint32_t)steps <= K) {
                        success_k[idx] += 1;
                    }
                }
            }

            // progress po jednej replikacii
            progress_t p;
            p.repl_index = htonl(r);
            p.repl_total = htonl(reps);
            send_to_client(MSG_PROGRESS, &p, sizeof(p));
        }

        // posli summary cells
        for (uint32_t yy = 0; yy < h; yy++) {
            for (uint32_t xx = 0; xx < w; xx++) {

                size_t idx = (size_t)yy * (size_t)w + (size_t)xx;

                int32_t cx = (int32_t)xx - (int32_t)center_x;
                int32_t cy = (int32_t)yy - (int32_t)center_y;

                int32_t value_fixed = 0;

                if (view == 0) {
                    double avg = (double)total_steps[idx] / (double)reps;
                    value_fixed = (int32_t)(avg * 100.0);
                } else {
                    double prob = (double)success_k[idx] / (double)reps;
                    value_fixed = (int32_t)(prob * 10000.0);
                }

                summary_cell_t cell;
                cell.x = (int32_t)htonl((uint32_t)cx);
                cell.y = (int32_t)htonl((uint32_t)cy);
                cell.value_fixed = (int32_t)htonl((uint32_t)value_fixed);

                send_to_client(MSG_SUMMARY_CELL, &cell, sizeof(cell));
            }
        }

        // koniec summary presne raz
        send_to_client(MSG_SUMMARY_DONE, NULL, 0);
        send_to_client(MSG_STATUS, "simulation finished",
                       (uint32_t)strlen("simulation finished"));

        free(total_steps);
        free(success_k);

        // oznac ze simulacia uz nebezi
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
    srand(time(NULL));

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
        // ak bol stary klient, zavrieme ho
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

                    printf("[server] START_SIM w=%u h=%u reps=%u K=%u\n", host.world_w, host.world_h, host.replications, host.K);

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
