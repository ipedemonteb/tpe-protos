#include "include/state_utils.h"
#include "../include/socks5_handler.h"
#include <sys/types.h>

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