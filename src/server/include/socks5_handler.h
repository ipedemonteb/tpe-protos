#ifndef SOCKS5HANDLER_H_
#define SOCKS5HANDLER_H_

#include <stdint.h>
#include <sys/socket.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <netdb.h>
#include "selector.h"
#include "buffer.h"
#include "stm.h"
#include "../../utils/logger.h"
#include "../states/include/state_hello.h"
#include "../states/include/state_connect.h"
#include "../states/include/state_auth.h"
#include "../states/include/state_request.h"
#include "../states/include/state_forward.h"

#define BUFFER_SIZE 4096
#define HOST_MAX_LEN 256
#define PORT_MAX_LEN 6

#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum {
    STATE_HELLO = 0,
    STATE_AUTH,
    STATE_REQUEST,
    STATE_CONNECT,
    STATE_FORWARDING,
    STATE_DONE,
    STATE_ERROR,
} socks5_state;

typedef struct socks5_connection {
    int client_fd;
    int origin_fd;
    char origin_host[HOST_MAX_LEN];
    char origin_port[PORT_MAX_LEN];
    uint8_t origin_atyp;
    struct addrinfo *origin_res;
    struct buffer read_buffer;
    struct buffer write_buffer;
    struct buffer origin_read_buffer;
    struct buffer origin_write_buffer;
    uint8_t raw_read[BUFFER_SIZE];
    uint8_t raw_write[BUFFER_SIZE];
    uint8_t raw_origin_read[BUFFER_SIZE];
    uint8_t raw_origin_write[BUFFER_SIZE];
    struct state_machine stm;
    uint8_t references;
} socks5_connection;

void accept_connection(struct selector_key *key);
void socks5_stm_read(struct selector_key *key);
void socks5_stm_write(struct selector_key *key);
void socks5_stm_block(struct selector_key *key);
void socks5_stm_close(struct selector_key *key);

static const struct fd_handler socks5_stm_handler = {
    .handle_read = (void (*)(struct selector_key *))socks5_stm_read,
    .handle_write = (void (*)(struct selector_key *))socks5_stm_write,
    .handle_block = (void (*)(struct selector_key *))socks5_stm_block,
    .handle_close = (void (*)(struct selector_key *))socks5_stm_close,
};

#endif