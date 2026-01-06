#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include  <netinet/in.h>
#include "server_net.h"
#include "protocol.h"
#include "protocol_io.h"

/*
    Globalna premenna:
    - server bezi, kym g_running == 1
*/
static int g_running = 1;

/*
    Pomocne vlakno simulacie.
    Zatial nerobi nic, len bezi.
    Neskor sem pride random walk logika.
*/
static void* simulation_thread(void* arg)
{
    (void)arg;

    while (g_running) {
        // simulacia bude riesena tu
        usleep(500 * 1000); // 500 ms
    }
    return NULL;
}

/*
    Precita port z argv.
    server <port>
*/
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

    // 1) spustenie server socketu
    int listen_fd = network_listen_on_port(port, 8);
    if (listen_fd < 0) {
        printf("[server] failed to start server\n");
        return 1;
    }

    printf("[server] running on port %d\n", port);

    // 2) spustenie simulacneho vlakna
    pthread_t sim_thread;
    if (pthread_create(&sim_thread, NULL, simulation_thread, NULL) != 0) {
        printf("[server] failed to create simulation thread\n");
        network_close_fd(&listen_fd);
        return 1;
    }

    // 3) hlavny server loop
    while (g_running) {
        printf("[server] waiting for client...\n");

        int client_fd = network_accept_client(listen_fd);
        if (client_fd < 0) {
            printf("[server] accept failed\n");
            continue;
        }

        printf("[server] client connected\n");

        /*
            4) loop pre komunikaciu s klientom
            Pouzivame binarny protokol (header + payload)
        */
        while (1) {
            msg_header_t hdr;
            void* payload = NULL;

            int r = protocol_receive(client_fd, &hdr, &payload);

            if (r == 0) {
                // klient sa odpojil
                printf("[server] client disconnected\n");
                break;
            }

            if (r < 0) {
                printf("[server] recv error\n");
                break;
            }

            // spracovanie spravy podla typu
            switch (hdr.type) {

                case MSG_HELLO: {
                    hello_t* h = (hello_t*)payload;
                    uint32_t version = ntohl(h->version);

                    printf("[server] HELLO version=%u\n", version);

                    // odpoved klientovi
                    protocol_send(client_fd, MSG_WELCOME, NULL, 0);
                    break;
                }

                case MSG_START_SIM: {
                    start_sim_t* s = (start_sim_t*)payload;

                    uint32_t w = ntohl(s->world_w);
                    uint32_t h = ntohl(s->world_h);
                    uint32_t reps = ntohl(s->replications);
                    uint32_t K = ntohl(s->K);

                    uint32_t pu = ntohl(s->p_up);
                    uint32_t pd = ntohl(s->p_down);
                    uint32_t pl = ntohl(s->p_left);
                    uint32_t pr = ntohl(s->p_right);

                    uint32_t mode = ntohl(s->mode);
                    uint32_t view = ntohl(s->view);

                    printf("[server] START_SIM\n");
                    printf("  world: %ux%u\n", w, h);
                    printf("  replications: %u\n", reps);
                    printf("  K: %u\n", K);
                    printf("  p: up=%u down=%u left=%u right=%u\n", pu, pd, pl, pr);
                    printf("  mode=%u view=%u\n", mode, view);

                    // zatial len potvrdime start
                    const char* msg = "simulation started";
                    protocol_send(client_fd, MSG_STATUS, msg, (uint32_t)strlen(msg));
                    break;
                }

                case MSG_SET_MODE: {
                    set_mode_t* m = (set_mode_t*)payload;
                    uint32_t mode = ntohl(m->mode);

                    printf("[server] SET_MODE %u\n", mode);
                    break;
                }

                case MSG_SET_VIEW: {
                    set_view_t* v = (set_view_t*)payload;
                    uint32_t view = ntohl(v->view);

                    printf("[server] SET_VIEW %u\n", view);
                    break;
                }

                case MSG_STOP_SIM: {
                    printf("[server] STOP_SIM\n");
                    break;
                }

                case MSG_BYE: {
                    printf("[server] BYE\n");
                    free(payload);
                    payload = NULL;
                    goto disconnect_client;
                }

                default:
                    printf("[server] unknown message type %u\n", hdr.type);
                    break;
            }

            free(payload);
        }

disconnect_client:
        network_close_fd(&client_fd);
    }

    // ukoncenie servera
    g_running = 0;
    pthread_join(sim_thread, NULL);
    network_close_fd(&listen_fd);

    printf("[server] stopped\n");
    return 0;
}
