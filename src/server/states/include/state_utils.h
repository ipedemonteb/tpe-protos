#ifndef STATEUTILS_H_
#define STATEUTILS_H_

#include <stdint.h>
#include "../../include/buffer.h"

void write_response(struct buffer *write_buff, uint8_t status, uint8_t atyp);

#endif