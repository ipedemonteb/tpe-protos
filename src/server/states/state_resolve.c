#include "include/state_resolve.h"
#include "../include/socks5_handler.h"

static void *resolve_thread_func(void *arg) {
    struct selector_key *key = (struct selector_key *)arg;
    socks5_connection *conn = key->data;
    pthread_detach(pthread_self());

    struct addrinfo hints = {0};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    int rv = getaddrinfo(conn->origin_host, conn->origin_port, &hints, &conn->origin_res);

    conn->resolve_status = rv;

    selector_notify_block(key->s, conn->client_fd);
    free(key);
    return NULL;
}

unsigned resolve_init(struct selector_key *key) {
    socks5_connection *conn = key->data;
    conn->resolve_status = -1;
    struct selector_key *key_copy = malloc(sizeof(*key));
    memcpy(key_copy, key, sizeof(*key_copy));
    
    pthread_t tid;
    int err = pthread_create(&tid, NULL, resolve_thread_func, key_copy);
    if (err != 0) {
        log(ERROR, "pthread_create failed: %s", strerror(err));
        return STATE_ERROR;
    }

    return STATE_RESOLVE;
}

unsigned resolution_block(struct selector_key *key) {
    socks5_connection *conn = key->data;

    if (conn->resolve_status != 0) {
        log(ERROR, "getaddrinfo failed: %s", gai_strerror(conn->resolve_status));
        write_response(&conn->write_buffer, HOST_UNREACHABLE, conn->origin_atyp, conn->origin_host, conn->origin_port);
        selector_set_interest_key(key, OP_WRITE);
        return STATE_REQUEST;
    }

    conn->origin_res_it = conn->origin_res;

    if (conn->origin_res_it == NULL) {
        log(ERROR, "No addresses found");
        add_user_site(conn->username, conn->origin_port, conn->origin_host, false);
        write_response(&conn->write_buffer, HOST_UNREACHABLE, conn->origin_atyp, "", conn->origin_port);
        selector_set_interest_key(key, OP_WRITE);
        return STATE_REQUEST;
    }

    add_user_site(conn->username, conn->origin_port, conn->origin_host, true);

    log(INFO, "Resolution successful for %s:%s", conn->origin_host, conn->origin_port);

    selector_set_interest_key(key, OP_NOOP);
    return connect_write(key);
}