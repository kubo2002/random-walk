#include "../headers/server_net.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Vytvori socket, nabinduje ho a pocuva
int network_listen_on_port(int port, int backlog)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    // Nech mozme port hneď znova použiť
    int yes = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, backlog) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

// Caka kym sa niekto pripoji
int network_accept_client(int listen_fd)
{
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }
    return client_fd;
}

// Posle vsetko, co je v buffri (aj keby to send rozsekal)
ssize_t network_send_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char*)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("send");
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}

// Precita presne tolko bajtov, kolko chceme
ssize_t network_receive_all(int fd, void *buf, size_t len)
{
    char *p = buf;
    size_t recvd = 0;

    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            return -1;
        }
        if (n == 0) return 0; // Odpojil sa
        recvd += (size_t)n;
    }
    return (ssize_t)recvd;
}

// Bezpecne zavrie socket
void network_close_fd(int *fd)
{
    if (fd != NULL && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}
