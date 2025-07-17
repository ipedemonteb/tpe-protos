#ifndef STATERESOLVE_H_
#define STATERESOLVE_H_

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../utils/include/selector.h"

unsigned resolution_block(struct selector_key *key);
unsigned resolve_init(struct selector_key *key);

#endif