#ifndef STATEAUTH_H_
#define STATEAUTH_H_

#include "../../utils/include/selector.h"
#include "../../utils/include/user_auth_utils.h"
#include "../../include/metrics.h"

unsigned auth_read(struct selector_key *key);
unsigned auth_write(struct selector_key *key);
unsigned auth_failed_write(struct selector_key *key);

#endif