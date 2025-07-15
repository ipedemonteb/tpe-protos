#ifndef STATEHELLO_H_
#define STATEHELLO_H_

#include "../../utils/include/selector.h"

unsigned hello_read(struct selector_key *key);
unsigned hello_write(struct selector_key *key);
unsigned hello_to_auth_write(struct selector_key *key);

#endif