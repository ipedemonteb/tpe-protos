#include "include/socks5_handler.h"
#include <netdb.h>
#include <stdlib.h>

void finish(struct selector_key *key);

static const struct state_definition socks5_states[] = {
    {
        .state = STATE_HELLO,
        .on_read_ready = hello_read,
        .on_write_ready = hello_write,
    },
    {
        .state = STATE_AUTH,
        .on_read_ready = auth_read,
        .on_write_ready = NULL,
    },
    {
        .state = STATE_REQUEST,
        .on_read_ready = request_read,
        .on_write_ready = request_write,
    },
    {
        .state = STATE_CONNECT,
        .on_write_ready = connect_write,
    },
    {
        .state = STATE_FORWARDING,
        .on_read_ready = forward_read,
        .on_write_ready = forward_write,
    },
    {
        .state = STATE_DONE,
    },
    {
        .state = STATE_ERROR,
    }
};

void socks5_stm_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    const socks5_state current_state = stm_handler_read(&connection->stm, key);
    if (current_state == STATE_ERROR || current_state == STATE_DONE) {
        finish(key);
    }
}

void socks5_stm_write(struct selector_key *key) {
    socks5_connection *connection = key->data;
    const socks5_state current_state = stm_handler_write(&connection->stm, key);
    if (current_state == STATE_ERROR || current_state == STATE_DONE) {
        finish(key);
    }
}

void socks5_stm_block(struct selector_key *key) {
    socks5_connection *connection = key->data;
    const socks5_state current_state = stm_handler_block(&connection->stm, key);
    if (current_state == STATE_ERROR || current_state == STATE_DONE) {
        finish(key);
    }
}

void socks5_stm_close(struct selector_key *key) {
    socks5_connection *connection = key->data;
    if (connection != NULL) {
        if(connection->references == 1) {
            if(connection->origin_res != NULL) {
                freeaddrinfo(connection->origin_res);
            }
            free(connection);
        } else {
            connection->references--;
        }
    }
}

void accept_connection(struct selector_key *key) {
    int server_fd = key->fd;
    fd_selector selector = key->data;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Accept a new connection
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if(client_fd < 0) {
        log(ERROR, "accept() failed: %s", strerror(errno));
        close(client_fd);
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
    buffer_init(&new_connection->origin_read_buffer,  BUFFER_SIZE, new_connection->raw_origin_read);
    buffer_init(&new_connection->origin_write_buffer, BUFFER_SIZE, new_connection->raw_origin_write);

    new_connection->stm = (struct state_machine){
        .states = socks5_states,
        .max_state = STATE_ERROR,
        .initial = STATE_HELLO,
        .current = NULL,
    };
    stm_init(&new_connection->stm);

    new_connection->references = 1;

    // Register the client socket with the selector
    if(selector_register(selector, client_fd, &socks5_stm_handler, OP_READ, new_connection) != SELECTOR_SUCCESS) {
        log(ERROR, "selector_register() failed for client");
        close(client_fd);
        free(new_connection);
        return;
    }

    log(INFO, "New client connected: fd=%d", client_fd);
}

void finish(struct selector_key *key) {
    socks5_connection *connection = key->data;
    const int fds[] = {
        connection->client_fd,
        connection->origin_fd
    };
    for(int i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(selector_unregister_fd(key->s, fds[i]) != SELECTOR_SUCCESS) {
                log(ERROR, "Failed to unregister fd %d: %s", fds[i], strerror(errno));
                abort();
            }
            //close(fds[i]);
        }
    }
}