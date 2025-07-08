#ifndef STATEAUTH_H_
#define STATEAUTH_H_

#include "../../include/selector.h"
#include "../../include/socks5_handler.h"
#include "../../include/user_auth_utils.h"

unsigned auth_read(struct selector_key *key);
unsigned auth_write(struct selector_key *key);
unsigned auth_failed_write(struct selector_key *key);

#endif