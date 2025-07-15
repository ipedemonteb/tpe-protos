#include "../include/socks5_handler.h"
#include "include/state_utils.h"
#include "../../monitor/metrics.h"

int get_ipv4_address(struct buffer *buff, char *host, char *port);
int get_domain_name(char *host, char *port, struct buffer *buff);
int get_ipv6_address(struct buffer *buff, char *host, char *port) ;
int get_resolution(const char *host, const char *port, struct addrinfo **res);

static void add_user_site(const char * user, const char * port, const char *site, int success) {
    char * destination_host = malloc(HOST_MAX_LEN);
    if (destination_host == NULL) {
        log(ERROR, "Failed to allocate memory for destination host");
        return;
    }
    strncpy(destination_host, site, HOST_MAX_LEN - 1);
    destination_host[HOST_MAX_LEN - 1] = '\0';
    char * destination_port = malloc(PORT_MAX_LEN);
    if (destination_port == NULL) {
        log(ERROR, "Failed to allocate memory for destination port");
        free(destination_host);
        return;
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

    // Resolve the address
    struct addrinfo *res;
    if (get_resolution(host, port, &res) != 0) {
        log(ERROR, "Failed to resolve host %s:%s", host, port);
        write_response(&connection->write_buffer, HOST_UNREACHABLE, atyp, host, port);
        selector_set_interest_key(key, OP_WRITE);
        return STATE_REQUEST;
    }

    connection->origin_res = res;
    connection->origin_res_it = res;

    selector_set_interest_key(key, OP_NOOP);
    return connect_write(key);
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

int get_resolution(const char *host, const char *port, struct addrinfo **res) {
    struct addrinfo addr_criteria = {0};
    addr_criteria.ai_family = AF_UNSPEC; // IPv4 or IPv6
    addr_criteria.ai_socktype = SOCK_STREAM; // TCP
    addr_criteria.ai_protocol = IPPROTO_TCP; // TCP protocol

    return getaddrinfo(host, port, &addr_criteria, res);
}