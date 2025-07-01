#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../utils/logger.h"
#include "buffer.h"
#include "selector.h"
#include "socks5_handler.h"


#define SOCKS5_VERSION 0x05
#define AUTH_VERSION 0x01
#define NO_AUTH_METHOD 0x00
#define IPV4_ATYP 0x01
#define DOMAIN_NAME_ATYP 0x03
#define IPV6_ATYP 0x04
#define RSV 0x00

unsigned hello_read(struct selector_key *key);
unsigned hello_write(struct selector_key *key);
unsigned auth_read(struct selector_key *key);
unsigned request_read(struct selector_key *key);
unsigned request_write(struct selector_key *key);
unsigned forward_read(struct selector_key *key);
unsigned forward_write(struct selector_key *key);
unsigned error_state(struct selector_key *key);
unsigned done_state(struct selector_key *key);
void get_domain_name_and_port(char *host, char *port, struct buffer *read_buff, uint8_t len);
void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp);
int open_connection(const char *host, const char *port);

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
        .state = STATE_RESPONSE,
        .on_read_ready = forward_read,
        .on_write_ready = forward_write,
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
        .on_read_ready = NULL,
        .on_write_ready = NULL,
    }
};

unsigned socks5_stm_read(struct selector_key *key) {
    socks5_connection *conn = key->data;
    return stm_handler_read(&conn->stm, key);
}

unsigned socks5_stm_write(struct selector_key *key) {
    socks5_connection *conn = key->data;
    return stm_handler_write(&conn->stm, key);
}

static const struct fd_handler socks5_stm_handler = {
    .handle_read = (void (*)(struct selector_key *))socks5_stm_read,
    .handle_write = (void (*)(struct selector_key *))socks5_stm_write,
    // @TODO: MISING HANDLERS
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

    // Register the client socket with the selector
    if(selector_register(selector, client_fd, &socks5_stm_handler, OP_READ, new_connection) != SELECTOR_SUCCESS) {
        log(ERROR, "selector_register() failed for client");
        close(client_fd);
        free(new_connection);
        return;
    }

    log(INFO, "New client connected: fd=%d", client_fd);
}

unsigned hello_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    if(connection == NULL) {
        log(ERROR, "Connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return STATE_ERROR;
    }

    size_t n;
    uint8_t *ptr = buffer_write_ptr(&connection->read_buffer, &n);
    ssize_t read = recv(connection->client_fd, ptr, n, 0);
    if(read < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return STATE_HELLO;
        }
        log(ERROR, "recv() failed: %s", strerror(errno));
        return STATE_ERROR;
    } else if(read == 0) {
        log(INFO, "Client closed connection during HELLO");
        return STATE_DONE;
    }

    buffer_write_adv(&connection->read_buffer, read);

    size_t available;
    uint8_t *read_ptr = buffer_read_ptr(&connection->read_buffer, &available);
    if (available < 2) {
        return STATE_HELLO;
    }

    if(buffer_can_read(&connection->read_buffer)) {
        uint8_t version = buffer_read(&connection->read_buffer);
        if(version != SOCKS5_VERSION) {
            log(ERROR, "Unsupported SOCKS version: %d", version);
            return STATE_ERROR;
        }
        uint8_t nmethods = buffer_read(&connection->read_buffer);
        for (int i = 0; i < nmethods && buffer_can_read(&connection->read_buffer); i++) {
            // Methods, not used for the moment
            buffer_read(&connection->read_buffer);
        }

        // Build the response
        buffer_reset(&connection->write_buffer);
        buffer_write(&connection->write_buffer, SOCKS5_VERSION);
        buffer_write(&connection->write_buffer, NO_AUTH_METHOD);
    }
    selector_set_interest_key(key, OP_WRITE);
    return STATE_HELLO;
}

unsigned hello_write(struct selector_key *key) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_read_ptr(&connection->write_buffer, &n);
    ssize_t sent = send(connection->client_fd, ptr, n, 0);
    if(sent < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return STATE_HELLO;
        }
        log(ERROR, "send() failed in hello_write: %s", strerror(errno));
        return STATE_ERROR;
    }
    buffer_read_adv(&connection->write_buffer, sent);
    if(buffer_can_read(&connection->write_buffer)) {
        return STATE_HELLO;
    }
    selector_set_interest_key(key, OP_READ);
    return STATE_REQUEST;
}

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

unsigned request_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_write_ptr(&connection->read_buffer, &n);

    ssize_t read = recv(connection->client_fd, ptr, n, 0);
    if (read < 0) {
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

    if(buffer_read(&connection->read_buffer) != SOCKS5_VERSION) {
        log(ERROR, "Unsupported SOCKS version in request");
        return STATE_ERROR;
    }
    // Only support CONNECT command for now
    uint8_t cmd = buffer_read(&connection->read_buffer);
    if(cmd != 0x01) {
        log(ERROR, "Unsupported command: %d", cmd);
        return STATE_ERROR;
    }
    // Reserved
    buffer_read(&connection->read_buffer);

    char host[256] = {0};
    char port[6] = {0};
    uint8_t atyp = buffer_read(&connection->read_buffer);
    switch(atyp) {
        case IPV4_ATYP: {
            uint8_t ip_bytes[4];
            for (int i = 0; i < 4; i++) {
                ip_bytes[i] = buffer_read(&connection->read_buffer);
            }
            snprintf(host, sizeof(host), "%u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            uint16_t port_num = buffer_read(&connection->read_buffer) << 8;
            port_num |= buffer_read(&connection->read_buffer);
            snprintf(port, 6, "%u", port_num);
            break;
        }
        case DOMAIN_NAME_ATYP: {
            uint8_t domain_length = buffer_read(&connection->read_buffer);
            get_domain_name_and_port(host, port, &connection->read_buffer, domain_length);
            break;
        }
        case IPV6_ATYP:
            break;
        default:
            log(ERROR, "Unsupported address type: %d", atyp);
            return STATE_ERROR;
    }
    log(INFO, "Connecting to %s:%s", host, port);

    // Open connection to the origin server
    connection->origin_fd = open_connection(host, port);

    if(selector_fd_set_nio(connection->origin_fd) == -1) {
        log(ERROR, "Failed to set origin_fd to non-blocking");
        close(connection->origin_fd);
        return STATE_ERROR;
    }

    if(selector_register(key->s, connection->origin_fd, &socks5_stm_handler, OP_READ, connection) != SELECTOR_SUCCESS) {
        log(ERROR, "Failed to register origin_fd");
        close(connection->origin_fd);
        return STATE_ERROR;
    }

    write_response(&connection->write_buffer, 0x00, atyp);
    selector_set_interest_key(key, OP_WRITE);
    return STATE_REQUEST;
}

unsigned request_write(struct selector_key *key) {
    socks5_connection *conn = key->data;
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_read_ptr(&connection->write_buffer, &n);
    ssize_t sent = send(connection->client_fd, ptr, n, 0);
    if(sent < 0) {
        log(ERROR, "send() failed: %s", strerror(errno));
        return STATE_ERROR;
    }
    buffer_read_adv(&connection->write_buffer, sent);
    if(buffer_can_read(&connection->write_buffer)) {
        return STATE_REQUEST;
    }

    if(selector_set_interest(key->s, conn->client_fd, OP_READ) != SELECTOR_SUCCESS) {
        log(ERROR, "Failed to set OP_READ on client fd");
        return STATE_ERROR;
    }

    if(selector_set_interest(key->s, conn->origin_fd, OP_READ) != SELECTOR_SUCCESS) {
        log(ERROR, "Failed to set OP_READ on origin fd");
        return STATE_ERROR;
    }

    log(INFO, "Request sent, switching to forward state");
    return STATE_FORWARDING;
}

unsigned forward_read(struct selector_key *key) {
    socks5_connection *conn = key->data;
    const bool from_client = key->fd == conn->client_fd;

    struct buffer *dst_buf = from_client ? &conn->origin_write_buffer : &conn->write_buffer;
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
    size_t n;
    uint8_t *ptr = buffer_read_ptr(src_buf, &n);

    ssize_t sent = send(key->fd, ptr, n, 0);
    if(sent < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) return STATE_FORWARDING;
        log(ERROR, "send() failed in forward_write: %s", strerror(errno));
        return STATE_ERROR;
    }

    buffer_read_adv(src_buf, sent);

    if(!buffer_can_read(src_buf)) {
        if(selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS) {
            log(ERROR, "Failed to set OP_READ in forward_write");
            return STATE_ERROR;
        }
    }
    log(INFO, "Data sent to %s", to_client ? "client" : "origin");
    return STATE_FORWARDING;
}

void get_domain_name_and_port(char *host, char *port, struct buffer *read_buff, uint8_t len) {
    for(int i = 0; i < len && buffer_can_read(read_buff); i++) {
        host[i] = buffer_read(read_buff);
    }
    uint16_t port_num = buffer_read(read_buff) << 8;
    port_num |= buffer_read(read_buff);
    snprintf(port, 6, "%u", port_num);
}

void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp) {
    buffer_reset(write_buff);
    buffer_write(write_buff, SOCKS5_VERSION);
    buffer_write(write_buff, status);
    buffer_write(write_buff, RSV);
    buffer_write(write_buff, atyp);

    switch(atyp) {
        case IPV4_ATYP:
            buffer_write(write_buff, 0);
            buffer_write(write_buff, 0);
            buffer_write(write_buff, 0);
            buffer_write(write_buff, 0);
            break;
        case DOMAIN_NAME_ATYP:
            buffer_write(write_buff, 0);
            break;
        case IPV6_ATYP:
            for (int i = 0; i < 16; i++) buffer_write(write_buff, 0);
            break;
    }

    buffer_write(write_buff, 0); // BND.PORT (2 bytes)
    buffer_write(write_buff, 0);
}

int open_connection(const char *host, const char *port) {
    struct addrinfo addrCriteria = {0};
    addrCriteria.ai_family = AF_UNSPEC; // IPv4 or IPv6
    addrCriteria.ai_socktype = SOCK_STREAM; // TCP
    addrCriteria.ai_protocol = IPPROTO_TCP; // TCP protocol

    struct addrinfo *res;
    int err = getaddrinfo(host, port, &addrCriteria, &res);
    if(err != 0) {
        log(ERROR, "getaddrinfo() failed: %s", gai_strerror(err));
        return -1;
    }
    
    int sockfd = -1;
    for(struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(sockfd < 0) {
            log(ERROR, "socket() failed: %s", strerror(errno));
            continue;
        }

        if(connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            log(ERROR, "connect() failed: %s", strerror(errno));
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break; // Successfully connected
    }

    log(INFO, "Connected to %s:%s", host, port);
    freeaddrinfo(res);
    return sockfd;
}