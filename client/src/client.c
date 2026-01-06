#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include "client_net.h"
#include "../server/headers/server_net.h"
#include "protocol.h"
#include "protocol_io.h"

static const char* read_ip(int argc, char** argv)
{
    // client <ip> <port>
    // default: 127.0.0.1
    if (argc < 2) return "127.0.0.1";
    return argv[1];
}

static int read_port(int argc, char** argv) {
    if (argc < 3) {
        return 5555;
    }

    int p = atoi(argv[2]);

    if (p <= 0 || p > 65535) {
        return 5555;
    }

    return p;
}

static int send_hello(int fd)
{
    hello_t h;
    h.version = htonl(1);

    return protocol_send(fd, MSG_HELLO, &h, sizeof(h));
}

static int send_start_sim(int fd)
{
    // toto su len default hodnoty, aby sme videli, ze protokol funguje
    start_sim_t s;

    s.world_w = htonl(11);
    s.world_h = htonl(11);
    s.replications = htonl(5);
    s.K = htonl(20);

    // pravdepodobnosti v promile (0..1000), suma musi byt 1000
    s.p_up = htonl(250);
    s.p_down = htonl(250);
    s.p_left = htonl(250);
    s.p_right = htonl(250);

    // mode: 0=interactive, 1=summary
    s.mode = htonl(1); // summary
    // view: 0=avg_steps, 1=prob_k
    s.view = htonl(0); // avg_steps

    return protocol_send(fd, MSG_START_SIM, &s, sizeof(s));
}

static int send_bye(int fd)
{
    return protocol_send(fd, MSG_BYE, NULL, 0);
}

int main(int argc, char** argv)
{
    const char* ip = read_ip(argc, argv);
    int port = read_port(argc, argv);

    printf("[client] connecting to %s:%d ...\n", ip, port);

    int fd = net_connect_to_server(ip, port);
    if (fd < 0) {
        printf("[client] connect failed\n");
        return 1;
    }

    printf("[client] connected\n");

    // HELLO
    if (send_hello(fd) < 0) {
        printf("[client] failed to send HELLO\n");
        close(fd);
        return 1;
    }

    // caka na WELCOME
    {
        msg_header_t hdr;
        void* payload = NULL;

        int r = protocol_receive(fd, &hdr, &payload);
        if (r <= 0) {
            printf("[client] failed to receive WELCOME\n");
            close(fd);
            return 1;
        }

        if (hdr.type == MSG_WELCOME) {
            printf("[client] got WELCOME\n");
        } else {
            printf("[client] unexpected msg type %u\n", hdr.type);
        }

        free(payload);
    }

    // START_SIM
    if (send_start_sim(fd) < 0) {
        printf("[client] failed to send START_SIM\n");
        close(fd);
        return 1;
    }
    printf("[client] START_SIM sent\n");

    /*
        cita odpovede zo servera
        Zatial viem, ze server posiela MSG_STATUS po START_SIM.
    */
    printf("[client] waiting for server messages (press ENTER to send BYE)\n");

    {
        msg_header_t hdr;
        void* payload = NULL;

        int r = protocol_receive(fd, &hdr, &payload);
        if (r <= 0) {
            printf("[client] server closed or error\n");
            close(fd);
            return 1;
        }

        if (hdr.type == MSG_STATUS) {
            // payload je text
            printf("[client] STATUS: %.*s\n", (int)hdr.payload_len, (char*)payload);
        } else {
            printf("[client] got msg type %u (len=%u)\n", hdr.type, hdr.payload_len);
        }

        free(payload);
    }

    // teraz uz fakt len pockame na enter a posleme BYE
    printf("[client] press ENTER to disconnect...\n");
    getchar();

    send_bye(fd);
    close(fd);

    printf("[client] disconnected\n");
    return 0;
}
