#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "client_net.h"
#include "../server/headers/server_net.h"
#include "protocol.h"
#include "protocol_io.h"
#include "render.h"

static int g_fd = -1;
static int g_running = 1;
static int g_summary_active = 0;   // 1 = cakame na summary pre aktualny start
static int g_current_view = 0;     // 0=avg, 1=prob
/*
    Receiver thread:
    - stale cita spravy zo servera a vypisuje ich
*/
static void* receiver_thread(void* arg)
{
    (void)arg;

    while (g_running) {
        msg_header_t hdr;
        void* payload = NULL;

        int r = protocol_receive(g_fd, &hdr, &payload);

        if (r == 0) {
            printf("[client] server disconnected\n");
            g_running = 0;
            break;
        }
        if (r < 0) {
            printf("[client] recv error\n");
            g_running = 0;
            break;
        }

        if (hdr.type == MSG_WELCOME) {
            printf("[client] WELCOME\n");
        }
        else if (hdr.type == MSG_STATUS) {
            printf("[client] STATUS: %.*s\n", (int)hdr.payload_len, (char*)payload);
        }
        else if (hdr.type == MSG_PROGRESS) {
            progress_t* p = (progress_t*)payload;
            uint32_t i = ntohl(p->repl_index);
            uint32_t total = ntohl(p->repl_total);
            printf("[client] PROGRESS %u/%u\n", i, total);
        }
        else if (hdr.type == MSG_SUMMARY_DONE) {
            if (g_summary_active) {
                printf("[client] SUMMARY_DONE\n");
                render_summary_end(g_current_view);
                g_summary_active = 0;
            }
        }
        else if (hdr.type == MSG_SUMMARY_CELL) {
            summary_cell_t* c = (summary_cell_t*)payload;

            int32_t x = (int32_t)ntohl((uint32_t)c->x);
            int32_t y = (int32_t)ntohl((uint32_t)c->y);
            int32_t v = (int32_t)ntohl((uint32_t)c->value_fixed);

            render_summary_cell((int)x, (int)y, (int)v);
        }

        else {
            printf("[client] msg type=%u len=%u\n", hdr.type, hdr.payload_len);
        }

        free(payload);
    }

    return NULL;
}

/*
    Input thread:
    - cita prikazy z klavesnice a posiela ich serveru
    - prikazy:
        start
        mode 0
        mode 1
        view 0
        view 1
        stop
        bye
*/
static void* input_thread(void* arg)
{
    (void)arg;

    char line[256];

    while (g_running) {
        printf("[client] command> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            g_running = 0;
            break;
        }

        // odstran \n
        line[strcspn(line, "\r\n")] = 0;

        if (strcmp(line, "start") == 0) {
            start_sim_t s;

            int w = 11;
            int h = 11;
            int replications = 10; // avg_steps
            int K = 20;
            s.world_w = htonl(11);
            s.world_h = htonl(11);
            s.replications = htonl((uint32_t)(replications));
            s.K = htonl((uint32_t)K);
            s.p_up = htonl(250);
            s.p_down = htonl(250);
            s.p_left = htonl(250);
            s.p_right = htonl(250);
            s.mode = htonl(1); // summary
            s.view = htonl((uint32_t)g_current_view);

            g_summary_active = 1;
            render_summary_begin(w, h, g_current_view);
            protocol_send(g_fd, MSG_START_SIM, &s, sizeof(s));
        }
        else if (strncmp(line, "mode ", 5) == 0) {
            int m = atoi(line + 5);
            set_mode_t mm;
            mm.mode = htonl((uint32_t)m);
            protocol_send(g_fd, MSG_SET_MODE, &mm, sizeof(mm));
        }
        else if (strncmp(line, "view ", 5) == 0) {
            int v = atoi(line + 5);
            if (v != 0 && v != 1) {
                printf("[client] view must be 0 or 1\n");
                continue;
            }

            g_current_view = v;
            set_view_t vv;
            vv.view = htonl((uint32_t)v);
            protocol_send(g_fd, MSG_SET_VIEW, &vv, sizeof(vv));

            printf("[client] view set to %d\n", v);
        }
        else if (strcmp(line, "stop") == 0) {
            protocol_send(g_fd, MSG_STOP_SIM, NULL, 0);
        }
        else if (strcmp(line, "bye") == 0) {
            protocol_send(g_fd, MSG_BYE, NULL, 0);
            g_running = 0;
            break;
        }
        else {
            printf("[client] commands: start | mode 0/1 | view 0/1 | stop | bye\n");
        }
    }

    return NULL;
}

static const char* read_ip(int argc, char** argv) {
    if (argc < 2) {
        return "127.0.0.1";
    }
    return argv[1];
}

static int read_port(int argc, char** argv)
{
    if (argc < 3) return 5555;
    int p = atoi(argv[2]);
    if (p <= 0 || p > 65535) return 5555;
    return p;
}

int main(int argc, char** argv)
{
    const char* ip = read_ip(argc, argv);
    int port = read_port(argc, argv);

    printf("[client] connecting to %s:%d...\n", ip, port);

    g_fd = net_connect_to_server(ip, port);
    if (g_fd < 0) {
        printf("[client] connect failed\n");
        return 1;
    }

    // HELLO
    hello_t h;
    h.version = htonl(1);
    protocol_send(g_fd, MSG_HELLO, &h, sizeof(h));

    // start threads
    pthread_t t_recv, t_in;

    pthread_create(&t_recv, NULL, receiver_thread, NULL);
    pthread_create(&t_in, NULL, input_thread, NULL);

    pthread_join(t_in, NULL);
    g_running = 0; // nech receiver skonci
    shutdown(g_fd, SHUT_RDWR);
    pthread_join(t_recv, NULL);

    close(g_fd);
    printf("[client] done\n");
    return 0;
}
