#include "include/state_forward.h"
#include "../include/socks5_handler.h"
#include "../../monitor/metrics.h"

unsigned forward_read(struct selector_key *key) {
    socks5_connection *conn = key->data;
    const bool from_client = key->fd == conn->client_fd;

    struct buffer *dst_buf = from_client ? &conn->origin_write_buffer : &conn->write_buffer;
    if (!buffer_can_write(dst_buf)) {
        if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS) {
            log(ERROR, "Failed to set OP_NOOP");
            return STATE_ERROR;
        }
        return STATE_FORWARDING;
    }

    size_t n;
    uint8_t *ptr = buffer_write_ptr(dst_buf, &n);
    ssize_t read_bytes = recv(key->fd, ptr, n, 0);
    if(read_bytes < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) return STATE_FORWARDING;
        log(ERROR, "recv() failed in forward_read: %s", strerror(errno));
        return STATE_ERROR;
    } else if(read_bytes == 0) {
        log(INFO, "%s closed the connection", from_client ? "Client" : "Origin");
        return STATE_DONE;
    }

    buffer_write_adv(dst_buf, read_bytes);
    metrics_bytes_transferred(read_bytes);

    int other_fd = from_client ? conn->origin_fd : conn->client_fd;
    if(selector_set_interest(key->s, other_fd, OP_WRITE) != SELECTOR_SUCCESS) {
        log(ERROR, "Failed to set OP_WRITE on peer");
        return STATE_ERROR;
    }
    log(INFO, "Data received from %s, forwarding to %s", 
        from_client ? "client" : "origin", 
        from_client ? "origin" : "client");
    return STATE_FORWARDING;
}

unsigned forward_write(struct selector_key *key) {
    socks5_connection *conn = key->data;
    const bool to_client = key->fd == conn->client_fd;

    struct buffer *src_buf = to_client ? &conn->write_buffer : &conn->origin_write_buffer;
    if (!buffer_can_read(src_buf)) {
        if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS) {
            log(ERROR, "Failed to set OP_NOOP in forward_write");
            return STATE_ERROR;
        }
        return STATE_FORWARDING;
    }

    size_t n;
    uint8_t *ptr = buffer_read_ptr(src_buf, &n);
    ssize_t sent = send(key->fd, ptr, n, 0);
    if(sent < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) return STATE_FORWARDING;
        log(ERROR, "send() failed in forward_write: %s", strerror(errno));
        return STATE_ERROR;
    }

    buffer_read_adv(src_buf, sent);
    metrics_bytes_transferred(sent);

    if(!buffer_can_read(src_buf)) {
        if(selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
            log(ERROR, "Failed to set OP_READ in forward_write");
            return STATE_ERROR;
        }
    }
    log(INFO, "Data sent to %s", to_client ? "client" : "origin");
    return STATE_FORWARDING;
}