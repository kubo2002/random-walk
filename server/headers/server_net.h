#ifndef SERVER_NET_H
#define SERVER_NET_H

#include <stddef.h>
#include <stdio.h>

int network_listen_on_port(int port, int backlog);
int network_accept_client(int listen_fd);

ssize_t network_send_all(int fd, const void *buf, size_t len);
ssize_t network_receive_all(int fd, void *buf, size_t len);

void network_close_fd(int *fd);

#endif
