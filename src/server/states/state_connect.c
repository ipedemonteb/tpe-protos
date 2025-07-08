#include "include/state_connect.h"
#include "../include/socks5_handler.h"

unsigned connect_write(struct selector_key *key) {
    socks5_connection *connection = key->data;
    int err;
    socklen_t len = sizeof(err);

    if (getsockopt(connection->origin_fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
        log(ERROR, "connect_write: connection failed: %s", strerror(err));
        close(connection->origin_fd);
        if (connection->origin_res) freeaddrinfo(connection->origin_res);
        return STATE_ERROR;
    }

    if (selector_set_interest(key->s, connection->client_fd, OP_WRITE) != SELECTOR_SUCCESS) {
        return STATE_ERROR;
    }

    write_response(&connection->write_buffer, 0x00, IPV4_ATYP);  // o DOMAIN/IPv6 según corresponda
    if (connection->origin_res) {
        freeaddrinfo(connection->origin_res);
        connection->origin_res = NULL;
    }

    return STATE_REQUEST;
}
