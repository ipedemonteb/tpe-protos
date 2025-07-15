#ifndef SERVERUTILS_H_
#define SERVERUTILS_H_

#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include "selector.h"
#include "../../../shared/include/logger.h"
#include "../../../shared/include/util.h"


// Create, bind, and listen a new TCP server socket
int setup_TCP_server_socket(const char *service);

#endif 
