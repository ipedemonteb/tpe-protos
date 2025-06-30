#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../utils/logger.h"
#include "buffer.h"
#include "socks5_handler.h"


#define SOCKS5_VERSION 0x05
#define NO_AUTH_METHOD 0x00
#define IPV4_ATYP 0x01
#define DOMAIN_NAME_ATYP 0x03
#define IPV6_ATYP 0x04
#define RSV 0x00

unsigned hello_read(struct selector_key *key);
unsigned hello_write(struct selector_key *key);
unsigned request_read(struct selector_key *key);
unsigned request_write(struct selector_key *key);
unsigned forward_read(struct selector_key *key);
unsigned forward_write(struct selector_key *key);
unsigned error_state(struct selector_key *key);
unsigned done_state(struct selector_key *key);
uint16_t get_domain_name_and_port(char *host, struct buffer *read_buff);
void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp);
int open_connection(socks5_connection *conn, const char *host, uint16_t port, fd_selector selector);

static const struct state_definition socks5_states[] = {
    [STATE_HELLO_READ] = {
        .state = STATE_HELLO_READ,
        .on_read_ready = hello_read,
        .on_write_ready = NULL,
    },
    [STATE_HELLO_WRITE] = {
        .state = STATE_HELLO_WRITE,
        .on_read_ready = NULL,
        .on_write_ready = hello_write,
    },
    [STATE_REQUEST_READ] = {
        .state = STATE_REQUEST_READ,
        .on_read_ready = request_read,
        .on_write_ready = NULL,
    },
    [STATE_REQUEST_WRITE] = {
        .state = STATE_REQUEST_WRITE,
        .on_read_ready = NULL,
        .on_write_ready = request_write,
    },
    [STATE_FORWARD_READ] = {
        .state = STATE_FORWARD_READ,
        .on_read_ready = forward_read,
        .on_write_ready = NULL,
    },
    [STATE_FORWARD_WRITE] = {
        .state = STATE_FORWARD_WRITE,
        .on_read_ready = NULL,
        .on_write_ready = forward_write,
    },
    [STATE_DONE] = {
        .state = STATE_DONE,
        .on_read_ready = NULL,
        .on_write_ready = NULL,
    },
    [STATE_ERROR] = {
        .state = STATE_ERROR,
        .on_read_ready = NULL,
        .on_write_ready = NULL,
    }
};

unsigned socks5_stm_read(struct selector_key *key) {
    socks5_connection *conn = key->data;
    return stm_handler_read(&conn->stm, key);
}

unsigned socks5_stm_write(struct selector_key *key) {
    socks5_connection *conn = key->data;
    return stm_handler_write(&conn->stm, key);
}

static const struct fd_handler socks5_stm_handler = {
    .handle_read = (void (*)(struct selector_key *))socks5_stm_read,
    .handle_write = (void (*)(struct selector_key *))socks5_stm_write,
};

void accept_connection(struct selector_key *key) {
    int server_fd = key->fd;
    fd_selector selector = key->data;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Accept a new connection
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if(client_fd < 0) {
        log(ERROR, "accept() failed: %s", strerror(errno));
        return;
    }

    // Set the client socket to non-blocking
    if(selector_fd_set_nio(client_fd) == -1) {
        log(ERROR, "Failed to set client socket to non-blocking: %s", strerror(errno));
        close(client_fd);
        return;
    }

    // Client structure
    socks5_connection *new_connection = malloc(sizeof(socks5_connection));
    if(new_connection == NULL) {
        log(ERROR, "Failed to allocate memory for new connection");
        close(client_fd);
        return;
    }
    new_connection->client_fd = client_fd;
    new_connection->origin_fd = -1;

    buffer_init(&new_connection->read_buffer,  BUFFER_SIZE, new_connection->raw_read);
    buffer_init(&new_connection->write_buffer, BUFFER_SIZE, new_connection->raw_write);
    buffer_init(&new_connection->origin_buffer, BUFFER_SIZE, new_connection->raw_origin);

    new_connection->stm = (struct state_machine){
        .states = socks5_states,
        .max_state = sizeof(socks5_states) / sizeof(socks5_states[0]) - 1,
        .initial = STATE_HELLO_READ,
        .current = NULL,
    };
    stm_init(&new_connection->stm);

    // Register the client socket with the selector
    if(selector_register(selector, client_fd, &socks5_stm_handler, OP_READ, new_connection) != SELECTOR_SUCCESS) {
        log(ERROR, "selector_register() failed for client");
        close(client_fd);
        free(new_connection);
        return;
    }

    log(INFO, "New client connected: fd=%d", client_fd);
}

unsigned hello_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    if(connection == NULL) {
        log(ERROR, "Connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return STATE_ERROR;
    }

    size_t n;
    uint8_t *ptr = buffer_write_ptr(&connection->read_buffer, &n);

    ssize_t read = recv(connection->client_fd, ptr, n, 0);
    if(read < 0) {
        log(ERROR, "recv() failed: %s", strerror(errno));
        return STATE_ERROR;
    } else if (read == 0) {
        log(INFO, "Client closed connection");
        return STATE_DONE;
    }

    buffer_write_adv(&connection->read_buffer, read);

    if(buffer_can_read(&connection->read_buffer)) {
        uint8_t version = buffer_read(&connection->read_buffer);
        if(version != SOCKS5_VERSION) {
            log(ERROR, "Unsupported SOCKS version: %d", version);
            return STATE_ERROR;
        }
        uint8_t nmethods = buffer_read(&connection->read_buffer);
        for (int i = 0; i < nmethods && buffer_can_read(&connection->read_buffer); i++) {
            // Methods, not used for the moment
            buffer_read(&connection->read_buffer);
        }

        // Build the response
        buffer_reset(&connection->write_buffer);
        buffer_write(&connection->write_buffer, SOCKS5_VERSION);
        buffer_write(&connection->write_buffer, NO_AUTH_METHOD);
    }
    selector_set_interest_key(key, OP_WRITE);
    return STATE_HELLO_WRITE;
}

unsigned hello_write(struct selector_key *key) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_read_ptr(&connection->write_buffer, &n);
    ssize_t sent = send(connection->client_fd, ptr, n, 0);
    if(sent < 0) {
        log(ERROR, "send() failed: %s", strerror(errno));
        return -1;
    }
    buffer_read_adv(&connection->write_buffer, sent);
    selector_set_interest_key(key, OP_READ);
    return STATE_REQUEST_READ;
}

unsigned request_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_write_ptr(&connection->read_buffer, &n);

    ssize_t read = recv(connection->client_fd, ptr, n, 0);
    if(read < 0) {
        log(ERROR, "recv() failed: %s", strerror(errno));
        return -1;
    }
    buffer_write_adv(&connection->read_buffer, read);

    if(!buffer_can_read(&connection->read_buffer)) {
        return 0;
    }

    if(buffer_read(&connection->read_buffer) != SOCKS5_VERSION) {
        log(ERROR, "Unsupported SOCKS version in request");
        return -1;
    }
    // Only support CONNECT command for now
    uint8_t cmd = buffer_read(&connection->read_buffer);
    if(cmd != 0x01) {
        log(ERROR, "Unsupported command: %d", cmd);
        return -1;
    }
    buffer_read(&connection->read_buffer); //reserved

    char host [256] = {0};
    uint16_t port = 0;
    uint8_t atyp = buffer_read(&connection->read_buffer);
    switch(atyp) {
        case IPV4_ATYP:
            break;
        case DOMAIN_NAME_ATYP:
            port = get_domain_name_and_port(host, &connection->read_buffer);
            break;
        case IPV6_ATYP:
            break;
        default:
            log(ERROR, "Unsupported address type: %d", atyp);
            return -1;
    }
    log(INFO, "Connecting to %s:%d", host, port);

    if(open_connection(connection, host, port, ) != 0) {
        log(ERROR, "Failed to open connection to %s:%d", host, port);
        return -1;
    }

    write_response(&connection->write_buffer, 0x00, atyp);
    return STATE_REQUEST_WRITE;
}

uint16_t get_domain_name_and_port(char *host, struct buffer *read_buff) {
    uint8_t domain_length = buffer_read(read_buff);
    for(int i = 0; i < domain_length && buffer_can_read(read_buff); i++) {
        host[i] = buffer_read(read_buff);
    }
    uint16_t port = buffer_read(read_buff) << 8;
    port |= buffer_read(read_buff);
    return port;
}

void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp) {
    // @TODO: Finish
    buffer_reset(write_buff);
    buffer_write(write_buff, SOCKS5_VERSION);
    buffer_write(write_buff, status);
    buffer_write(write_buff, RSV);
    buffer_write(write_buff, atyp);
    buffer_write(write_buff, 0);
    buffer_write(write_buff, 0);
    buffer_write(write_buff, 0);
    buffer_write(write_buff, 0);
    buffer_write(write_buff, 0);
    buffer_write(write_buff, 0);
}

int open_connection(socks5_connection *conn, const char *host, uint16_t port, fd_selector selector) {
    
    return 0;
}

unsigned request_write(struct selector_key *key) {
    return STATE_FORWARD_READ;
}

unsigned forward_read(struct selector_key *key) {
    return STATE_FORWARD_WRITE;
}

unsigned forward_write(struct selector_key *key) {
    return STATE_FORWARD_READ;
}