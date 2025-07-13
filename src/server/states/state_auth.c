#include "include/state_auth.h"

unsigned auth_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
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

    if(!buffer_can_read(&connection->read_buffer)) {
        return STATE_REQUEST;
    }

    if(buffer_read(&connection->read_buffer) != AUTH_VERSION) {
        log(ERROR, "Unsupported authentication version");
        return -1;
    }

    char username[256] = {0};
    uint8_t ulen = buffer_read(&connection->read_buffer);
    for(int i = 0; i < ulen && buffer_can_read(&connection->read_buffer); i++) {
        username[i] = buffer_read(&connection->read_buffer);
    }

    char password[256] = {0};
    uint8_t plen = buffer_read(&connection->read_buffer);
    for(int i = 0; i < plen && buffer_can_read(&connection->read_buffer); i++) {
        password[i] = buffer_read(&connection->read_buffer);
    }

    log(INFO, "Received authentication request for user: %s", username);

    // Check credentials
    uint8_t auth_status = credentials_are_valid(username, password);
    
    // Build the response
    buffer_reset(&connection->write_buffer);
    buffer_write(&connection->write_buffer, AUTH_VERSION);
    buffer_write(&connection->write_buffer, auth_status);
    
    selector_set_interest_key(key, OP_WRITE);
    
    if (auth_status != 0x00) {
        log(INFO, "Invalid credentials for user: %s", username);
        return auth_failed_write(key);
    }

    log(INFO, "Authentication successful for user: %s", username);
    return auth_write(key);    
}

static unsigned auth_write_aux(struct selector_key *key, uint8_t auth_status, unsigned current_state) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_read_ptr(&connection->write_buffer, &n);
    if(n == 0) {
        return STATE_AUTH;
    }
    ssize_t sent = send(connection->client_fd, ptr, n, 0);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return current_state;
        }
        log(ERROR, "send() failed in auth_write: %s (errno: %d)", strerror(errno), errno);
        return STATE_ERROR;
    }
    buffer_read_adv(&connection->write_buffer, sent);
    if (buffer_can_read(&connection->write_buffer)) {
        return current_state;
    }

    if (auth_status != 0x00) {
        return STATE_DONE;
    }

    selector_set_interest_key(key, OP_READ);
    return STATE_REQUEST;
}
                         
unsigned auth_write(struct selector_key *key) {
    return auth_write_aux(key, 0x00, STATE_AUTH);
}

unsigned auth_failed_write(struct selector_key *key) {
    return auth_write_aux(key, 0x01, STATE_AUTH_FAILED);
}