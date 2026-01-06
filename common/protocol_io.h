#ifndef PROTOCOL_IO_H
#define PROTOCOL_IO_H

#include <stdint.h>
#include "protocol.h"

// posle header + payload (payload moze byt NULL ak len header)
int protocol_send(int fd, uint32_t type, const void* payload, uint32_t payload_len);

// precita header, alokuje payload (ak payload_len > 0)
// caller musi free(payload_out)
int protocol_receive(int fd, msg_header_t* hdr_out, void** payload_out);

// pomocne konverzie (host<->network)
uint32_t u32_hton(uint32_t v);
uint32_t u32_ntoh(uint32_t v);

#endif
