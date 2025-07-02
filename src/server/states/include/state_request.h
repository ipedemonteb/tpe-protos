#ifndef STATEREQUEST_H_
#define STATEREQUEST_H_

#include <stdint.h>
#include "../../include/selector.h"
#include "state_utils.h"

unsigned request_read(struct selector_key *key);
unsigned request_write(struct selector_key *key);

#endif