#include "protocol_io.h"
#include "../server/headers/server_net.h"   // net_send_all, net_recv_all (uz mas)
#include <stdlib.h>
#include <arpa/inet.h>

// helpery pre
uint32_t u32_hton(uint32_t v) {
    return htonl(v);
}
uint32_t u32_ntoh(uint32_t v) {
    return ntohl(v);
}

int protocol_send(int fd, uint32_t type, const void* payload, uint32_t payload_len) {
    msg_header_t h;
    h.type = htonl(type);
    h.payload_len = htonl(payload_len);

    if (network_send_all(fd, &h, sizeof(h)) < 0) {
        return -1;
    }

    if (payload_len > 0 && payload != NULL) {
        if (network_send_all(fd, payload, payload_len) < 0) {
            return -1;
        }
    }
    return 0;
}

int protocol_receive(int fd, msg_header_t* hdr_out, void** payload_out) {
    *payload_out = NULL;

    msg_header_t h_net;
    ssize_t n = network_receive_all(fd, &h_net, sizeof(h_net));

    if (n == 0) {
        return 0; // klient sa odpojil
    }

    if (n < 0) {
        return -1;
    }

    hdr_out->type = ntohl(h_net.type);
    hdr_out->payload_len = ntohl(h_net.payload_len);

    if (hdr_out->payload_len == 0) {
        return 1; // ok, len header
    }

    void* buf = malloc(hdr_out->payload_len);
    if (!buf) {
        return -1;
    }

    n = network_receive_all(fd, buf, hdr_out->payload_len);

    if (n == 0) {
        free(buf);
        return 0;
    }
    if (n < 0) {
        free(buf);
        return -1;
    }

    *payload_out = buf;
    return 1;
}
