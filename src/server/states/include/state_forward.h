#ifndef STATEFORWARD_H_
#define STATEFORWARD_H_

#include "../../utils/include/selector.h"

unsigned forward_read(struct selector_key *key);
unsigned forward_write(struct selector_key *key);

#endif