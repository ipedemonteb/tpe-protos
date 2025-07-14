#ifndef STATEUTILS_H_
#define STATEUTILS_H_

#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "../../include/buffer.h"

#define SOCKS5_VERSION 0x05
#define AUTH_VERSION 0x01
#define NO_AUTH_METHOD 0x00
#define IPV4_ATYP 0x01
#define DOMAIN_NAME_ATYP 0x03
#define IPV6_ATYP 0x04
#define RSV 0x00
#define CONNECT_CMD 0x01
#define SERV_ERROR 0x01
#define FIREWALL_ERROR 0x02
#define NETWORK_UNREACHABLE 0x03
#define EXPIRED_TTL 0x06
#define UNSUPPORTED_COMMAND 0x07
#define UNSUPPORTED_ATYP 0x08
#define HOST_UNREACHABLE 0x04
#define CONNECTION_REFUSED 0x05
#define SUCCESS 0x00

void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp, char *host, char *port);

#endif