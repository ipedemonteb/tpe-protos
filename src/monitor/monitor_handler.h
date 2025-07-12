#ifndef MONITORHANDLER_H_
#define MONITORHANDLER_H_

#include <stdint.h>
#include <sys/socket.h>
#include "../server/include/selector.h"
#include "../server/include/buffer.h"

#define MONITOR_BUFFER_SIZE 1024
#define MAX_COMMAND_SIZE 256
#define MAX_RESPONSE_SIZE 1024

typedef enum {
    MONITOR_HANDSHAKE_READ,
    MONITOR_HANDSHAKE_WRITE,
    MONITOR_COMMAND_READ,
    MONITOR_COMMAND_WRITE,
    MONITOR_DONE,
    MONITOR_ERROR_STATE,
} monitor_state;

typedef struct monitor_connection {
    monitor_state state;
    int client_fd;
    struct buffer read_buffer;
    struct buffer write_buffer;
    uint8_t raw_read[MONITOR_BUFFER_SIZE];
    uint8_t raw_write[MONITOR_BUFFER_SIZE];
    char command[MAX_COMMAND_SIZE];
    char response[MAX_RESPONSE_SIZE];
    int handshake_done;
} monitor_connection;

void monitor_accept_connection(struct selector_key *key);
void monitor_read(struct selector_key *key);
void monitor_write(struct selector_key *key);
void monitor_close(struct selector_key *key);

int handle_monitor_handshake_read(struct buffer *read_buff, int fd);
int handle_monitor_handshake_write(struct buffer *write_buff, int fd);
int handle_monitor_command_read(struct buffer *read_buff, int fd, struct buffer *write_buff, monitor_connection *conn);
int handle_monitor_command_write(struct buffer *write_buff, int fd);

void handle_stats_command(monitor_connection *conn);
void handle_connections_command(monitor_connection *conn);
void handle_users_command(monitor_connection *conn);
void handle_config_command(monitor_connection *conn, const char *args);

#endif 