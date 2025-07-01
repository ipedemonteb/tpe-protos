#ifndef TCPSERVERUTIL_H_
#define TCPSERVERUTIL_H_

#include <stdio.h>
#include <sys/socket.h>
#include "selector.h"


// Create, bind, and listen a new TCP server socket
int setupTCPServerSocket(const char *service);

void echo_read(struct selector_key *key);
void echo_close(struct selector_key *key);

// Handle new TCP client
int handleTCPEchoClient(int clntSocket);

#endif 
