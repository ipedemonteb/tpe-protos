#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include "../utils/logger.h"
#include "tcpServerUtil.h"
#include "selector.h"
#include "socks5_handler.h"
#include "../monitor/monitor_handler.h"
#include "../monitor/metrics.h"

#define INIT_ELEMENTS 1024
#define DEFAULT_SOCKS_PORT "1080"
#define DEFAULT_MONITOR_PORT "9090"

int init_server(char *socks_port, char *monitor_port);

int main(int argc, char *argv[]) {
    char *socks_port = DEFAULT_SOCKS_PORT;
    char *monitor_port = DEFAULT_MONITOR_PORT;
    
    if (argc >= 2) {
        socks_port = argv[1];
    }
    if (argc >= 3) {
        monitor_port = argv[2];
    }

    return init_server(socks_port, monitor_port);
}

int init_server(char *socks_port, char *monitor_port) {

    metrics_init();
    
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

    // Initialize the SOCKS socket
    int socksSock = setupTCPServerSocket(socks_port);
    if (socksSock < 0 ) {
        log(FATAL, "Failed to initialize SOCKS socket");
        return 1;
    }

    // Initialize the monitor socket
    int monitorSock = setupTCPServerSocket(monitor_port);
    if (monitorSock < 0 ) {
        log(FATAL, "Failed to initialize monitor socket");
        close(socksSock);
        return 1;
    }

    // Make both FDs non-blocking
    selector_fd_set_nio(socksSock);
    selector_fd_set_nio(monitorSock);

    // Instance new selector
    fd_selector selector = selector_new(INIT_ELEMENTS);
    if(selector == NULL) {
        log(FATAL, "Failed to create selector");
        close(socksSock);
        close(monitorSock);
        return 1;
    }

    // Set SOCKS listen socket in the selector
    struct fd_handler socks_handler = {
        .handle_read = accept_connection,
    };
    if(selector_register(selector, socksSock, &socks_handler, OP_READ, selector) != SELECTOR_SUCCESS) {
        log(FATAL, "Failed to register SOCKS server socket");
        selector_destroy(selector);
        close(socksSock);
        close(monitorSock);
        return 1;
    }

    // Set monitor listen socket in the selector
    struct fd_handler monitor_handler = {
        .handle_read = monitor_accept_connection,
    };
    if(selector_register(selector, monitorSock, &monitor_handler, OP_READ, selector) != SELECTOR_SUCCESS) {
        log(FATAL, "Failed to register monitor server socket");
        selector_destroy(selector);
        close(socksSock);
        close(monitorSock);
        return 1;
    }

    log(INFO, "Server started - SOCKS on port %s, Monitor on port %s", socks_port, monitor_port);

    // Run forever
    while (1) {
        selector_select(selector);
    }

    selector_destroy(selector);
    selector_close();
    close(socksSock);
    close(monitorSock);
    return 0;
}