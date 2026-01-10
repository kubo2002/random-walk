#include "protocol_io.h"
#include "../server/headers/server_net.h"
#include <stdlib.h>
#include <arpa/inet.h>

// Posle spravu (hlavicku + payload)
int protocol_send(int fd, uint32_t type, const void* payload, uint32_t payload_len) {
    msg_header_t h;
    h.type = htonl(type);
    h.payload_len = htonl(payload_len);

    // Posleme hlavicku
    if (network_send_all(fd, &h, sizeof(h)) < 0) {
        return -1;
    }

    // A potom data, ak nejake su
    if (payload_len > 0 && payload != NULL) {
        if (network_send_all(fd, payload, payload_len) < 0) {
            return -1;
        }
    }
    return 0;
}

// Prijme spravu, alokuje pamat pre payload
int protocol_receive(int fd, msg_header_t* hdr_out, void** payload_out) {
    *payload_out = NULL;

    msg_header_t h_net;
    ssize_t n = network_receive_all(fd, &h_net, sizeof(h_net));

    if (n <= 0) return (int)n; // Odpojenie alebo chyba

    hdr_out->type = ntohl(h_net.type);
    hdr_out->payload_len = ntohl(h_net.payload_len);

    if (hdr_out->payload_len == 0) return 1;

    void* buf = malloc(hdr_out->payload_len);
    if (!buf) return -1;

    n = network_receive_all(fd, buf, hdr_out->payload_len);
    if (n <= 0) {
        free(buf);
        return (int)n;
    }

    *payload_out = buf;
    return 1;
}
