#include "include/state_connect.h"
#include "../include/socks5_handler.h"
#include "include/state_utils.h"
#include <netdb.h>

unsigned connect_write(struct selector_key *key) {
    socks5_connection *conn = key->data;

    while (conn->origin_res_it != NULL) {
        if (conn->origin_fd == -1) {
            conn->origin_fd = socket(conn->origin_res_it->ai_family,
                                     conn->origin_res_it->ai_socktype,
                                     conn->origin_res_it->ai_protocol);
            if (conn->origin_fd < 0) {
                conn->origin_res_it = conn->origin_res_it->ai_next;
                continue;
            }

            if (selector_fd_set_nio(conn->origin_fd) < 0) {
                log(INFO, "Could not make FD non blocking, moving to next one: %s", strerror(errno));
                close(conn->origin_fd);
                conn->origin_fd = -1;
                conn->origin_res_it = conn->origin_res_it->ai_next;
                continue;
            }

            if (connect(conn->origin_fd, conn->origin_res_it->ai_addr,
                        conn->origin_res_it->ai_addrlen) == 0) {

                if (selector_register(key->s, conn->origin_fd, &socks5_stm_handler, OP_WRITE, conn) != SELECTOR_SUCCESS) {
                    close(conn->origin_fd);
                    conn->origin_fd = -1;
                    return STATE_ERROR;
                }

                conn->references++;
                return STATE_CONNECT;
            }

            if (errno == EINPROGRESS) {
                if (selector_register(key->s, conn->origin_fd, &socks5_stm_handler, OP_WRITE, conn) != SELECTOR_SUCCESS) {
                    close(conn->origin_fd);
                    conn->origin_fd = -1;
                    return STATE_ERROR;
                }
                conn->references++;
                return STATE_CONNECT;
            }
            
            log(INFO, "connect() failed: %s", strerror(errno));
            close(conn->origin_fd);
            conn->origin_fd = -1;
            conn->origin_res_it = conn->origin_res_it->ai_next;
            continue;
        }

        int err;
        socklen_t len = sizeof(err);
        if (getsockopt(conn->origin_fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
            selector_unregister_fd(key->s, conn->origin_fd);
            close(conn->origin_fd);
            conn->origin_fd = -1;
            conn->origin_res_it = conn->origin_res_it->ai_next;
            continue;
        }


        log(INFO, "Connection Stablished");
        if (selector_set_interest(key->s, conn->client_fd, OP_WRITE) != SELECTOR_SUCCESS) {
            close(conn->origin_fd);
            conn->origin_fd = -1;
            return STATE_ERROR;
        }

        write_response(&conn->write_buffer, SUCCESS, conn->origin_atyp, conn->origin_host, conn->origin_port);

        freeaddrinfo(conn->origin_res);
        conn->origin_res = NULL;
        conn->origin_res_it = NULL;

        return STATE_REQUEST;
    }

    write_response(&conn->write_buffer, HOST_UNREACHABLE, conn->origin_atyp, conn->origin_host, conn->origin_port);
    selector_set_interest_key(key, OP_WRITE);
    
    if (conn->origin_res != NULL) {
        freeaddrinfo(conn->origin_res);
        conn->origin_res = NULL;
    }
    if (conn->origin_res_it != NULL) {
        freeaddrinfo(conn->origin_res_it);
        conn->origin_res_it = NULL;
    }

    return STATE_REQUEST;
}
