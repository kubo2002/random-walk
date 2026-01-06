#ifndef SERVER_NET_H
#define SERVER_NET_H

#include <stddef.h>
#include <stdio.h>

int net_listen_on_port(int port, int backlog);
int net_accept_client(int listen_fd);

ssize_t net_send_all(int fd, const void *buf, size_t len);
ssize_t net_recv_all(int fd, void *buf, size_t len);

void net_close_fd(int *fd);

#endif
