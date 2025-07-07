#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../utils/logger.h"
#include "buffer.h"
#include "selector.h"
#include "socks5_handler.h"
#include "../monitor/metrics.h"

#define SOCKS5_VERSION 0x05
#define NO_AUTH_METHOD 0x00

int handle_hello_read(struct buffer *read_buff, int fd, struct buffer *write_buff);
int handle_hello_write(struct buffer *write_buff, int fd);

static const struct fd_handler socks5_handler = {
    .handle_read = socks5_read,
    .handle_write = socks5_write,
    .handle_close = socks5_close,
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
    new_connection->state = HELLO_READ;
    new_connection->client_fd = client_fd;
    new_connection->origin_fd = -1;

    buffer_init(&new_connection->read_buffer,  BUFFER_SIZE, new_connection->raw_read);
    buffer_init(&new_connection->write_buffer, BUFFER_SIZE, new_connection->raw_write);
    buffer_init(&new_connection->origin_buffer, BUFFER_SIZE, new_connection->raw_origin);

    // Register the client socket with the selector
    if(selector_register(selector, client_fd, &socks5_handler, OP_READ, new_connection) != SELECTOR_SUCCESS) {
        log(ERROR, "selector_register() failed for client");
        close(client_fd);
        free(new_connection);
        return;
    }

    metrics_connection_start();
    
    char client_ip[INET6_ADDRSTRLEN];
    if (client_addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        //Esta función transforma la dirección IP binaria (s.sin_addr) en una cadena de texto, guardándola en client_ip
        inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof(client_ip));
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
        inet_ntop(AF_INET6, &s->sin6_addr, client_ip, sizeof(client_ip));
    }
    
    // @todo: añadir al usuario real cuando sea implementado
    metrics_add_user("anonymous", client_ip);

    log(INFO, "New SOCKS client connected: fd=%d, ip=%s", client_fd, client_ip);
}

void socks5_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    if(connection == NULL) {
        log(ERROR, "Connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return;
    }

    switch(connection->state) {
        case HELLO_READ:
            if(handle_hello_read(&connection->read_buffer, key->fd, &connection->write_buffer) != 0) {
                connection->state = ERROR_STATE;
                selector_set_interest_key(key, OP_NOOP);
                return;
            }
            connection->state = HELLO_WRITE;
            selector_set_interest_key(key, OP_WRITE);
            break;
        case REQUEST_READ:
            break;
        case FORWARD_READ:
            break;
        default:
            log(ERROR, "Unknown state in socks5_read: %d", connection->state);
            connection->state = ERROR_STATE;
            selector_set_interest_key(key, OP_NOOP);
            break;
    }
}

void socks5_write(struct selector_key *key) {
    socks5_connection *connection = key->data;
    if(connection == NULL) {
        log(ERROR, "Connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return;
    }

    switch(connection->state) {
        case HELLO_WRITE:
            if(handle_hello_write(&connection->write_buffer, key->fd) != 0) {
                connection->state = ERROR_STATE;
                selector_set_interest_key(key, OP_NOOP);
                return;
            }
            if(!buffer_can_read(&connection->write_buffer)) {
                connection->state = REQUEST_READ;
                selector_set_interest_key(key, OP_READ);
            }
            break;
        case REQUEST_WRITE:
            break;
        case FORWARD_WRITE:
            break;
        default:
            log(ERROR, "Unexpected state in socks5_write: %d", connection->state);
            selector_set_interest_key(key, OP_NOOP);
            break;
    }
}

void socks5_close(struct selector_key *key) {
    if(key->data != NULL) {
        socks5_connection *connection = key->data;
        
        metrics_connection_end();
        
        // @todo: implementar para usuarios cuando los tengamos
        metrics_remove_user("anonymous");
        
        free(connection);
    }
    close(key->fd);
    log(INFO, "SOCKS client disconnected: fd=%d", key->fd);
}

int handle_hello_read(struct buffer *read_buff, int fd, struct buffer *write_buff) {
    size_t n;
    uint8_t *ptr = buffer_write_ptr(read_buff, &n);

    ssize_t read = recv(fd, ptr, n, 0);
    if(read < 0) {
        log(ERROR, "recv() failed: %s", strerror(errno));
        return -1;
    }
    buffer_write_adv(read_buff, read);

    if(buffer_can_read(read_buff)) {
        uint8_t version = buffer_read(read_buff);
        if(version != SOCKS5_VERSION) {
            log(ERROR, "Unsupported SOCKS version: %d", version);
            return -1;
        }
        uint8_t nmethods = buffer_read(read_buff);
        for (int i = 0; i < nmethods && buffer_can_read(read_buff); i++) {
            // Methods, not used for the moment
            buffer_read(read_buff);
        }

        // Build the response
        buffer_reset(write_buff);
        buffer_write(write_buff, SOCKS5_VERSION);
        buffer_write(write_buff, NO_AUTH_METHOD);
    }
    return 0;
}

int handle_hello_write(struct buffer *write_buff, int fd) {
    size_t n;
    uint8_t *ptr = buffer_read_ptr(write_buff, &n);
    ssize_t sent = send(fd, ptr, n, 0);
    if(sent < 0) {
        log(ERROR, "send() failed: %s", strerror(errno));
        return -1;
    }
    buffer_read_adv(write_buff, sent);
    return 0;
}
