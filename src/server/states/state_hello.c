#include "include/state_hello.h"
#include "../include/socks5_handler.h"
#include "../../monitor/metrics.h"

unsigned hello_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    if (connection == NULL) {
        log(ERROR, "Connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return STATE_ERROR;
    }

    size_t n;
    uint8_t *ptr = buffer_write_ptr(&connection->read_buffer, &n);
    ssize_t read = recv(connection->client_fd, ptr, n, 0);
    if (read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return STATE_HELLO;
        }
        log(ERROR, "recv() failed: %s", strerror(errno));
        return STATE_ERROR;
    } else if (read == 0) {
        log(INFO, "Client closed connection during HELLO");
        return STATE_DONE;
    }

    buffer_write_adv(&connection->read_buffer, read);

    size_t available;
    buffer_read_ptr(&connection->read_buffer, &available);
    if (available < 2) {
        return STATE_HELLO;
    }

    uint8_t version = buffer_read(&connection->read_buffer);
    if(version != SOCKS5_VERSION) {
        log(ERROR, "Unsupported SOCKS version: %d", version);
        return STATE_ERROR;
    }
    uint8_t nmethods = buffer_read(&connection->read_buffer);
    if (nmethods <= 0) {
        log(ERROR, "Invalid number of methods: %d", nmethods);
        return STATE_ERROR;
    }
    if ((uint8_t)available < (2 + nmethods)) {
        return STATE_HELLO;
    }
    uint8_t chosen_method = NO_AUTH_METHOD; // Default to no authentication
    for (int i = 0; i < nmethods && buffer_can_read(&connection->read_buffer); i++) {
        if (buffer_read(&connection->read_buffer) == AUTH_METHOD) {
            chosen_method = AUTH_METHOD;
        }
    }
    // Build the response
    buffer_reset(&connection->write_buffer);
    buffer_write(&connection->write_buffer, SOCKS5_VERSION);
    buffer_write(&connection->write_buffer, chosen_method);

    selector_set_interest_key(key, OP_WRITE);

    if (chosen_method == AUTH_METHOD) {
        log(INFO, "Client supports authentication, switching to AUTH state");
        
        return hello_to_auth_write(key);
    }
    
    strncpy(connection->username, "Anonymous", MAX_USERNAME_LEN);
    metrics_add_user(connection->username);
     
    return hello_write(key);
}

static unsigned hello_write_aux(struct selector_key *key, unsigned next_state, unsigned current_state) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_read_ptr(&connection->write_buffer, &n);
    if(n == 0) {
        return current_state;
    }
    ssize_t sent = send(connection->client_fd, ptr, n, 0);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return current_state;
        }
        log(ERROR, "send() failed in hello_write: %s (errno: %d)", strerror(errno), errno);
        return STATE_ERROR;
    }
    buffer_read_adv(&connection->write_buffer, sent);
    if (buffer_can_read(&connection->write_buffer)) {
        return current_state;
    }
    selector_set_interest_key(key, OP_READ);
    return next_state;
}

unsigned hello_write(struct selector_key *key) {
    return hello_write_aux(key, STATE_REQUEST, STATE_HELLO);
}

unsigned hello_to_auth_write(struct selector_key *key) {
    return hello_write_aux(key, STATE_AUTH, STATE_HELLO_TO_AUTH);
}