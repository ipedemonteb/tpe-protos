#include "include/state_auth.h"
#include "../include/socks5_handler.h"

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

    // Here will go the call to the function that checks the username and password
    uint8_t auth_status = 0x00; // Assume success for now

    // Build the response
    buffer_reset(&connection->write_buffer);
    buffer_write(&connection->write_buffer, AUTH_VERSION);
    buffer_write(&connection->write_buffer, auth_status);

    return STATE_REQUEST;
}