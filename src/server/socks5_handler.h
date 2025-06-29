#ifndef SOCKS5HANDLER_H_
#define SOCKS5HANDLER_H_

#include <stdint.h>
#include <sys/socket.h>
#include "selector.h"
#include "buffer.h"

#define BUFFER_SIZE 4096

typedef enum {
    HELLO_READ,
    HELLO_WRITE,
    REQUEST_READ,
    REQUEST_WRITE,
    FORWARD_READ,
    FORWARD_WRITE,
    DONE,
    ERROR_STATE,
} socks5_state;

typedef struct socks5_connection {
    socks5_state state;
    int client_fd;
    int origin_fd;
    struct buffer read_buffer;
    struct buffer write_buffer;
    struct buffer origin_buffer;
    uint8_t raw_read[BUFFER_SIZE];
    uint8_t raw_write[BUFFER_SIZE];
    uint8_t raw_origin[BUFFER_SIZE];
} socks5_connection;

void accept_connection(struct selector_key *key);
void socks5_read(struct selector_key *key);
void socks5_write(struct selector_key *key);
void socks5_close(struct selector_key *key);

#endif