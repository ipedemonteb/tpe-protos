#include "include/state_utils.h"

void error_handle(struct buffer *write_buff, uint8_t atyp);
void write_ipv4_address(struct buffer *write_buff, char *host);
void write_domain_address(struct buffer *write_buff, char *host);
void write_ipv6_address(struct buffer *write_buff, char *host);

void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp, char *host, char *port) {
    buffer_reset(write_buff);
    buffer_write(write_buff, SOCKS5_VERSION);
    buffer_write(write_buff, status);
    buffer_write(write_buff, RSV);
    buffer_write(write_buff, atyp);

    if(status == UNSUPPORTED_ATYP) {
        error_handle(write_buff, atyp);
        return;
    }

    switch(atyp) {
        case IPV4_ATYP:
            write_ipv4_address(write_buff, host);
            break;
        case DOMAIN_NAME_ATYP:
            write_domain_address(write_buff, host);
            break;
        case IPV6_ATYP:
            write_ipv6_address(write_buff, host);
            break;
        default:
            for (int i = 0; i < 4; i++) buffer_write(write_buff, 0x00);
            break;
    }

    uint16_t p = (uint16_t) atoi(port);
    buffer_write(write_buff, (p >> 8) & 0xFF);
    buffer_write(write_buff, p & 0xFF);
}

void error_handle(struct buffer *write_buff, uint8_t atyp) {
    switch(atyp) {
            case IPV4_ATYP:
                for (int i = 0; i < 4; i++) buffer_write(write_buff, 0x00);
                break;
            case DOMAIN_NAME_ATYP:
                buffer_write(write_buff, 0x00);
                break;
            case IPV6_ATYP:
                for (int i = 0; i < 16; i++) buffer_write(write_buff, 0x00);
                break;
            default:
                for (int i = 0; i < 4; i++) buffer_write(write_buff, 0x00);
                break;
        }
        buffer_write(write_buff, 0x00);
        buffer_write(write_buff, 0x00);
        return;
}

void write_ipv4_address(struct buffer *write_buff, char *host) {
    struct in_addr ipv4_addr;
    if (inet_pton(AF_INET, host, &ipv4_addr) == 1) {
        uint8_t *addr_bytes = (uint8_t*) &ipv4_addr;
        for (int i = 0; i < 4; i++) {
            buffer_write(write_buff, addr_bytes[i]);
        }
    } else {
        for (int i = 0; i < 4; i++) buffer_write(write_buff, 0x00);
    }
}

void write_domain_address(struct buffer *write_buff, char *host) {
    size_t len = strlen(host);
    if (len > 255) {
        len = 255;
    }
    buffer_write(write_buff, (uint8_t) len);
    for (size_t i = 0; i < len; i++) {
        buffer_write(write_buff, (uint8_t) host[i]);
    }
}

void write_ipv6_address(struct buffer *write_buff, char *host) {
    struct in6_addr ipv6_addr;
    if (inet_pton(AF_INET6, host, &ipv6_addr) == 1) {
        uint8_t *addr_bytes = (uint8_t*) &ipv6_addr;
        for (int i = 0; i < 16; i++) {
            buffer_write(write_buff, addr_bytes[i]);
        }
    } else {
        for (int i = 0; i < 16; i++) buffer_write(write_buff, 0x00);
    }
}