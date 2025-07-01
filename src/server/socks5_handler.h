#ifndef SOCKS5HANDLER_H_
#define SOCKS5HANDLER_H_

#include <stdint.h>
#include <sys/socket.h>
#include "selector.h"
#include "buffer.h"
#include "stm.h"

#define BUFFER_SIZE 4096
#define SOCKS5_VERSION 0x05
#define NO_AUTH_METHOD 0x00
#define IPV4_ATYP 0x01
#define DOMAIN_NAME_ATYP 0x03
#define IPV6_ATYP 0x04
#define RSV 0x00

typedef enum {
    STATE_HELLO = 0,
    STATE_REQUEST,
    STATE_RESPONSE,
    STATE_FORWARDING,
    STATE_DONE,
    STATE_ERROR,
} socks5_state;

typedef struct socks5_connection {
    int client_fd;
    int origin_fd;
    struct buffer read_buffer;
    struct buffer write_buffer;
    struct buffer origin_read_buffer;
    struct buffer origin_write_buffer;
    uint8_t raw_read[BUFFER_SIZE];
    uint8_t raw_write[BUFFER_SIZE];
    uint8_t raw_origin_read[BUFFER_SIZE];
    uint8_t raw_origin_write[BUFFER_SIZE];
    struct state_machine stm;
} socks5_connection;

void accept_connection(struct selector_key *key);
void socks5_read(struct selector_key *key);
void socks5_write(struct selector_key *key);
void socks5_close(struct selector_key *key);

#endif