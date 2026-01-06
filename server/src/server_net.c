#include "../headers/server_net.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Vytvori TCP server socket, nabinduje ho na dany port
 * a zacne pocuvat na prichadzajuce pripojenia.
 *
 * vracia:
 *  - file descriptor socketu
 *  - alebo -1 pri chybe
 */
int network_listen_on_port(int port, int backlog)
{
    // vytvorime TCP socket (IPv4, stream)
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    // povolime opakovane pouzitie portu
    // (aby server slo hned restartovat bez cakania)
    int yes = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }

    // nastavime strukturu adresy servera
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;                // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // pocuva na vsetkych lokalnych IP
    addr.sin_port = htons((unsigned short)port);

    // bind = priradime socket ku konkretnemu portu
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    // listen = zacneme pocuvat prichadzajuce pripojenia
    if (listen(listen_fd, backlog) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    // ak sme sa sem dostali, server socket je pripraveny
    return listen_fd;
}

/*
 * Caka na pripojenie klienta.
 * Funkcia accept() blokuje, kym sa nejaky klient nepripoji.
 *
 * vracia:
 *  - client socket fd
 *  - alebo -1 pri chybe
 */
int network_accept_client(int listen_fd)
{
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }

    return client_fd;
}

/*
 * Posle cely buffer cez socket.
 * send() nemusi poslat vsetky data naraz,
 * preto posielam v cykle, kym sa neposle vsetko.
 */
ssize_t network_send_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char*)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            // ak bolo send prerusene signalom, skusime znova
            if (errno == EINTR) continue;
            perror("send");
            return -1;
        }

        // n == 0 by normalne nemalo nastat, ale pre istotu
        if (n == 0) {
            return -1;
        }

        sent += (size_t)n;
    }

    return (ssize_t)sent;
}

/*
 * Precita presne len bajtov zo socketu.
 * recv() tiez nemusi precitat vsetko naraz,
 * preto citame v cykle.
 */
ssize_t network_receive_all(int fd, void *buf, size_t len)
{
    char *p = buf;
    size_t recvd = 0;

    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n < 0) {
            // prerusenie signalom -> skusime znova
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }

        // n == 0 znamena, ze klient sa korektne odpojil
        if (n == 0) {
            return 0;
        }
        recvd += (size_t)n;
    }

    return (ssize_t)recvd;
}

/*
 * Bezpecne zatvori file descriptor.
 * Pouzivame pointer, aby sme ho mohli rovno nastavit na -1.
 */
void network_close_fd(int *fd)
{
    if (fd == NULL) {
        return;
    }

    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}
