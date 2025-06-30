#ifndef SOCKS5HANDLER_H_
#define SOCKS5HANDLER_H_

#include <stdint.h>
#include <sys/socket.h>
#include "selector.h"
#include "buffer.h"
#include "stm.h"
#include "user_auth_utils.h"

#define BUFFER_SIZE 4096

typedef enum {
    STATE_HELLO_READ = 0,
    STATE_HELLO_WRITE,
    STATE_REQUEST_READ,
    STATE_REQUEST_WRITE,
    STATE_FORWARD_READ,
    STATE_FORWARD_WRITE,
    STATE_DONE,
    STATE_ERROR,
    STATE_AUTH_READ,
    STATE_AUTH_WRITE,
} socks5_state;

typedef struct socks5_connection {
    int client_fd;
    int origin_fd;
    struct buffer read_buffer;
    struct buffer write_buffer;
    struct buffer origin_buffer;
    uint8_t raw_read[BUFFER_SIZE];
    uint8_t raw_write[BUFFER_SIZE];
    uint8_t raw_origin[BUFFER_SIZE];
    struct state_machine stm;
} socks5_connection;

void accept_connection(struct selector_key *key);
void socks5_read(struct selector_key *key);
void socks5_write(struct selector_key *key);
void socks5_close(struct selector_key *key);

#endif