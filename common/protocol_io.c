#include "protocol_io.h"
#include "../server/headers/server_net.h"
#include <stdlib.h>
#include <arpa/inet.h>

/*
 * Pomocne funkcie na prevod medzi network a host byte order pre 32-bitove cisla.
 */
uint32_t u32_hton(uint32_t v) {
    return htonl(v);
}
uint32_t u32_ntoh(uint32_t v) {
    return ntohl(v);
}

/*
 * Posle spravu (hlavicku a payload) cez dany file descriptor.
 * Vracia 0 pri uspechu, -1 pri chybe.
 */
int protocol_send(int fd, uint32_t type, const void* payload, uint32_t payload_len) {
    msg_header_t h;
    h.type = htonl(type);
    h.payload_len = htonl(payload_len);

    // Najprv posleme hlavicku
    if (network_send_all(fd, &h, sizeof(h)) < 0) {
        return -1;
    }

    // Ak mame payload, posleme ho hned za hlavickou
    if (payload_len > 0 && payload != NULL) {
        if (network_send_all(fd, payload, payload_len) < 0) {
            return -1;
        }
    }
    return 0;
}

/*
 * Prijme spravu zo socketu. Najprv precita hlavicku, potom alokuje pamat
 * pre payload a precita ho tiez.
 * Vracia: 1 ak prisla sprava, 0 ak sa klient odpojil, -1 pri chybe.
 */
int protocol_receive(int fd, msg_header_t* hdr_out, void** payload_out) {
    *payload_out = NULL;

    msg_header_t h_net;
    ssize_t n = network_receive_all(fd, &h_net, sizeof(h_net));

    if (n == 0) {
        return 0; // Odpojenie
    }

    if (n < 0) {
        return -1; // Chyba
    }

    // Prevod z network byte order spat do host byte order
    hdr_out->type = ntohl(h_net.type);
    hdr_out->payload_len = ntohl(h_net.payload_len);

    // Ak sprava nema payload, koncime uspesne
    if (hdr_out->payload_len == 0) {
        return 1;
    }

    // Alokacia pamate pre payload (pozor, volajuci ju musi uvolnit!)
    void* buf = malloc(hdr_out->payload_len);
    if (!buf) {
        return -1;
    }

    // Precitame samotny payload
    n = network_receive_all(fd, buf, hdr_out->payload_len);

    if (n <= 0) {
        free(buf);
        return (n == 0) ? 0 : -1;
    }

    *payload_out = buf;
    return 1;
}
