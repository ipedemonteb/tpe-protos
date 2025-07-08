#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../utils/logger.h"
#include "include/server_utils.h"
#include "include/selector.h"
#include "include/socks5_handler.h"

#define INIT_ELEMENTS 1024
#define DEFAULT_PORT "1080"

static bool finished = false;

int init_server(char *serv_port);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log(FATAL, "Usage: %s <Server Port>", argv[0]);
    }

    char * serv_port = argv[1];

    // Nothing to read from stdin, omg free FD !
    close(STDIN_FILENO);

    // Disable buffering for stdout
    if(setvbuf(stdout, NULL, _IONBF, 0)) {
        perror("Unable to disable buffering");
        return -1;
    }

    // Disable buffering for stderr
    if (setvbuf(stderr, NULL, _IONBF, 0)) {
        perror("Unable to disable buffering");
        return -1;
    }

    return init_server(serv_port);
}

int init_server(char *serv_port) {
    int ret = 0;
    const char *err_msg = NULL;
    selector_status ss = SELECTOR_SUCCESS;
    fd_selector selector = NULL;

    // Initialize the socket
    int serv_sock = setup_TCP_server_socket(serv_port);
    if (serv_sock < 0 ) {
        err_msg = "Failed to initialize socket";
        goto finally;
    }

    // Selector Configuration
    struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        },
    };

    // Initialize the selector
    if (selector_init(&conf) != SELECTOR_SUCCESS) {
        err_msg = "Failed to initialize selector";
        goto finally;
    }

    // Make the FD non-blocking
    selector_fd_set_nio(serv_sock);

    // Instance new selector
    selector = selector_new(INIT_ELEMENTS);
    if (selector == NULL) {
        err_msg = "Failed to create selector";
        goto finally;
    }

    // Set listen socket in the selector
    struct fd_handler handler = {
        .handle_read = accept_connection,
    };
    ss = selector_register(selector, serv_sock, &handler, OP_READ, selector);
    if (ss != SELECTOR_SUCCESS) {
        err_msg = "Failed to register server socket";
        goto finally;
    }

    while (!finished) {
        err_msg = NULL;
        selector_select(selector);
        if (ss != SELECTOR_SUCCESS) {
            err_msg = "Selector select failed";
            goto finally;
        }
    }

    if (err_msg == NULL) {
        log(INFO, "Server stopped serving");
    }

finally:
    if (err_msg != NULL) {
        log(ERROR, "%s", err_msg);
        ret = -1;
    }
    if (selector != NULL) {
        selector_destroy(selector);
    }
    if (serv_sock >= 0) {
        close(serv_sock);
    }
    selector_close();
    return ret;
}