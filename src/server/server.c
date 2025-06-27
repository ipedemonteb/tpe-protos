#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "logger.h"
#include "tcpServerUtil.h"
#include "selector.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log(FATAL, "usage: %s <Server Port>", argv[0]);
    }

    char * servPort = argv[1];

    // Selector Configuration
    struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        },
    };

    // Initialize the selector
    if(selector_init(&conf) != SELECTOR_SUCCESS) {
        log(FATAL, "Failed to initialize selector");
        return 1;
    }

    // Initialize the socket
    int servSock = setupTCPServerSocket(servPort);
    if (servSock < 0 ) {
        log(FATAL, "Failed to initialize socket");
        return 1;
    }

    // Make the FD non-blocking
    selector_fd_set_nio(servSock);

    // Instance new selector
    fd_selector selector = selector_new(1024);
    if(selector == NULL) {
        log(FATAL, "Failed to create selector");
        return 1;
    }

    // Set listen socket in the selector
    struct fd_handler handler = {
        .handle_read = accept_connection,
    };
    if(selector_register(selector, servSock, &handler, OP_READ, selector) != SELECTOR_SUCCESS) {
        log(FATAL, "Failed to register server socket");
        selector_destroy(selector);
        close(servSock);
        return 1;
    }

    // Run forever
    while (1) {
        selector_select(selector);
    }

    selector_destroy(selector);
    selector_close();
    close(servSock);
    return 0;
}