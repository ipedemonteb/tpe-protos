#include <netdb.h>
#include <stdlib.h>
#include "include/socks5_handler.h"
#include "../monitor/metrics.h"

void finish(struct selector_key *key);

static const struct state_definition socks5_states[] = {
    {
        .state = STATE_HELLO,
        .on_read_ready = hello_read,
        .on_write_ready = hello_write,
    },
    {
        .state = STATE_HELLO_TO_AUTH,
        .on_read_ready = NULL,
        .on_write_ready = hello_to_auth_write,
    },
    {
        .state = STATE_AUTH,
        .on_read_ready = auth_read,
        .on_write_ready = auth_write,
    },
    {
        .state = STATE_AUTH_FAILED,
        .on_read_ready = NULL,
        .on_write_ready = auth_failed_write,
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
    if (connection == NULL) {
        return;
    }

    if (connection->origin_res != NULL) {
        if (connection->origin_fd>=0)freeaddrinfo(connection->origin_res);
        connection->origin_res = NULL;
    }

    if (connection->references > 0) {
        connection->references--;
    }

    if (connection->references == 0) {
        free(connection);
    }
}

void accept_connection(struct selector_key *key) {    int server_fd = key->fd;
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

    log(INFO, "New SOCKS client connected: fd=%d, ip=%s", client_fd, client_ip);
}

void finish(struct selector_key *key) {
    socks5_connection *connection = key->data;
    
    metrics_connection_end();

    //@todo: implementar para usuarios cuando los tengamos
    metrics_remove_user("anonymous");

    const int fds[] = {
        connection->client_fd,
        connection->origin_fd
    };
    for(int i = 0; i < (int)N(fds); i++) {
        if(fds[i] != -1) {
            if(selector_unregister_fd(key->s, fds[i]) != SELECTOR_SUCCESS) {
                log(ERROR, "Failed to unregister fd %d: %s", fds[i], strerror(errno));
                abort();
            }
            close(fds[i]);
        }
    }
}