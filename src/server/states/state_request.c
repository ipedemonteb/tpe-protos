#include "../include/socks5_handler.h"
#include "include/state_utils.h"
#include "../../monitor/metrics.h"

int get_ipv4_address(struct buffer *buff, char *host, char *port);
int get_domain_name(char *host, char *port, struct buffer *buff);
int get_ipv6_address(struct buffer *buff, char *host, char *port) ;
int open_socket(const char *host, const char *port, struct addrinfo **res);

static void add_user_site(const char * user, const char * port, const char *site, int success) {
    char * destination_host = malloc(HOST_MAX_LEN);
        if (destination_host == NULL) {
            log(ERROR, "Failed to allocate memory for destination host");
            return STATE_ERROR;
        }
        strncpy(destination_host, site, HOST_MAX_LEN - 1);
        destination_host[HOST_MAX_LEN - 1] = '\0';
        char * destination_port = malloc(PORT_MAX_LEN);
        if (destination_port == NULL) {
            log(ERROR, "Failed to allocate memory for destination port");
            free(destination_host);
            return STATE_ERROR;
        }
        strncpy(destination_port, port, PORT_MAX_LEN - 1);
        destination_port[PORT_MAX_LEN - 1] = '\0';
        metrics_add_user_site(user, destination_host, success, destination_port);
}

unsigned request_read(struct selector_key *key) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_write_ptr(&connection->read_buffer, &n);

    // @TODO: VER EL TEMA DE CONSUMIR DESTRUCTIVAMENTE
    ssize_t read = recv(connection->client_fd, ptr, n, 0);
    if (read < 0) {
        log(ERROR, "recv() failed: %s", strerror(errno));
        return STATE_ERROR;
    } else if (read == 0) {
        log(INFO, "Client closed connection");
        return STATE_DONE;
    }
    buffer_write_adv(&connection->read_buffer, read);

    if (buffer_readable_bytes(&connection->read_buffer) < 4) {
        return STATE_REQUEST;
    }

    if(buffer_read(&connection->read_buffer) != SOCKS5_VERSION) {
        log(ERROR, "Unsupported SOCKS version in request");
        return STATE_ERROR;
    }

    // Only support CONNECT command for now
    uint8_t cmd = buffer_read(&connection->read_buffer);
    // Reserved
    buffer_read(&connection->read_buffer);
    // ATYP (Address Type)
    uint8_t atyp = buffer_read(&connection->read_buffer);

    char host[HOST_MAX_LEN] = {0};
    char port[PORT_MAX_LEN] = {0};
    int result = -1;
    switch (atyp) {
        case IPV4_ATYP: {
            result = get_ipv4_address(&connection->read_buffer, host, port);
            break;
        }
        case DOMAIN_NAME_ATYP: {
            result = get_domain_name(host, port, &connection->read_buffer);
            break;
        }
        case IPV6_ATYP:
            result = get_ipv6_address(&connection->read_buffer, host, port);
            break;
        default:
            log(ERROR, "Unsupported address type: %d", atyp);
            write_response(&connection->write_buffer, UNSUPPORTED_ATYP, atyp, host, port);
            selector_set_interest_key(key, OP_WRITE);
            return STATE_REQUEST;
    }
    if (result < 0) {
        log(INFO, "Request message incomplete, waiting for more bytes");
        return STATE_REQUEST;
    }

    connection->origin_atyp = atyp;
    strncpy(connection->origin_host, host, HOST_MAX_LEN - 1);
    strncpy(connection->origin_port, port, PORT_MAX_LEN - 1);

    if(cmd != CONNECT_CMD) {
        log(ERROR, "Unsupported command: %d", cmd);
        write_response(&connection->write_buffer, HOST_UNREACHABLE, atyp, host, port);
        selector_set_interest_key(key, OP_WRITE);
        return STATE_REQUEST;
    }

    log(INFO, "Connecting to %s:%s", host, port);

    // Open connection to the origin server
    struct addrinfo *res;
    connection->origin_fd = open_socket(host, port, &res);
    if (connection->origin_fd == -1) {
        write_response(&connection->write_buffer, HOST_UNREACHABLE, atyp, host, port);
        selector_set_interest_key(key, OP_WRITE);
        //@todo: agregar el usuario
        add_user_site("anonymous", port, host, false);
        return STATE_REQUEST;
    }
    if (connection->origin_fd == -2) {
        write_response(&connection->write_buffer, SERV_ERROR, atyp, host, port);
        selector_set_interest_key(key, OP_WRITE);
        return STATE_REQUEST;
    }
    connection->origin_res = res;

    if (selector_fd_set_nio(connection->origin_fd) == -1) {
        close(connection->origin_fd);
        freeaddrinfo(res);
        return STATE_ERROR;
    }

    if (connect(connection->origin_fd, res->ai_addr, res->ai_addrlen) == -1) {
        if (errno != EINPROGRESS) {
            log(ERROR, "connect() failed: %s", strerror(errno));
            write_response(&connection->write_buffer, CONNECTION_REFUSED, atyp, host, port);
            //@todo: agregar el usuario
            add_user_site("anonymus", port, host, false);
            close(connection->origin_fd);
            freeaddrinfo(res);
            return STATE_REQUEST;
        }

        connection->references++;
        if (selector_register(key->s, connection->origin_fd, &socks5_stm_handler, OP_WRITE, connection) != SELECTOR_SUCCESS) {
            close(connection->origin_fd);
            freeaddrinfo(res);
            return STATE_ERROR;
        }

        selector_set_interest_key(key, OP_NOOP);
        return STATE_CONNECT;
    }

    freeaddrinfo(res);
    write_response(&connection->write_buffer, SUCCESS, atyp, host, port);
    add_user_site("anonymus", port, host, true);
    selector_set_interest_key(key, OP_WRITE);
    return request_write(key);
}

unsigned request_write(struct selector_key *key) {
    socks5_connection *connection = key->data;
    size_t n;
    uint8_t *ptr = buffer_read_ptr(&connection->write_buffer, &n);
    ssize_t sent = send(connection->client_fd, ptr, n, 0);
    if (sent < 0) {
        log(ERROR, "send() failed: %s", strerror(errno));
        return STATE_ERROR;
    }
    buffer_read_adv(&connection->write_buffer, sent);
    if (buffer_can_read(&connection->write_buffer)) {
        return STATE_REQUEST;
    }

    if (selector_set_interest(key->s, connection->client_fd, OP_READ) != SELECTOR_SUCCESS ||
        selector_set_interest(key->s, connection->origin_fd, OP_READ) != SELECTOR_SUCCESS
    ) {
        log(ERROR, "Failed to set OP_READ on fds");
        return STATE_ERROR;
    }

    log(INFO, "Request sent, switching to forward state");
    return STATE_FORWARDING;
}

int get_ipv4_address(struct buffer *buff, char *host, char *port) {
    if (buffer_readable_bytes(buff) < 6) {
        return -1;
    }

    uint8_t ip_bytes[4];
    for (int i = 0; i < 4; i++) {
        ip_bytes[i] = buffer_read(buff);
    }
    snprintf(host, HOST_MAX_LEN, "%u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

    uint16_t port_num = buffer_read(buff) << 8 | buffer_read(buff);
    snprintf(port, 6, "%u", port_num);
    return 0;
}

int get_domain_name(char *host, char *port, struct buffer *buff) {
    if (buffer_readable_bytes(buff) < 1) {
        return -1;
    }

    uint8_t len = buffer_read(buff);
    for(int i = 0; i < len && buffer_can_read(buff); i++) {
        host[i] = buffer_read(buff);
    }
    host[len] = '\0';

    uint16_t port_num = buffer_read(buff) << 8 | buffer_read(buff);
    snprintf(port, PORT_MAX_LEN, "%u", port_num);
    return 0;
}

int get_ipv6_address(struct buffer *buff, char *host, char *port) {
    if (buffer_readable_bytes(buff) < 18) {
        return -1;
    }

    uint8_t ip_bytes[16];
    for (int i = 0; i < 16; i++) {
        ip_bytes[i] = buffer_read(buff);
    }

    struct in6_addr addr;
    memcpy(&addr, ip_bytes, 16);
    if (inet_ntop(AF_INET6, &addr, host, 256) == NULL) {
        log(ERROR, "inet_ntop failed: %s", strerror(errno));
        return -1;
    }

    uint16_t port_num = buffer_read(buff) << 8 | buffer_read(buff);
    snprintf(port, PORT_MAX_LEN, "%u", port_num);
    return 0;
}

int open_socket(const char *host, const char *port, struct addrinfo **res) {
    struct addrinfo addr_criteria = {0};
    addr_criteria.ai_family = AF_UNSPEC; // IPv4 or IPv6
    addr_criteria.ai_socktype = SOCK_STREAM; // TCP
    addr_criteria.ai_protocol = IPPROTO_TCP; // TCP protocol

    int err = getaddrinfo(host, port, &addr_criteria, res);
    if (err != 0) {
        log(ERROR, "getaddrinfo() failed: %s", gai_strerror(err));
        return -1;
    }

    int sockfd = socket((*res)->ai_family, (*res)->ai_socktype, (*res)->ai_protocol);
    if (sockfd < 0) {
        log(ERROR, "socket() failed: %s", strerror(errno));
        freeaddrinfo(*res);
        *res = NULL;
        return -2;
    }

    return sockfd;
}