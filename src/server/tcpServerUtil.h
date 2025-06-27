#ifndef TCPSERVERUTIL_H_
#define TCPSERVERUTIL_H_

#include <stdio.h>
#include <sys/socket.h>
#include "selector.h"


// Create, bind, and listen a new TCP server socket
int setupTCPServerSocket(const char *service);

// Accept a new TCP connection on a server socket
int acceptTCPConnection(int servSock);

// Accept non blocking
void accept_connection(struct selector_key *key);
void echo_read(struct selector_key *key);
void echo_close(struct selector_key *key);

// Handle new TCP client
int handleTCPEchoClient(int clntSocket);

#endif 
