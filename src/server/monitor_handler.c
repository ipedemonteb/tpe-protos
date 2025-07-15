#include "include/monitor_handler.h"

//@todo: deberíamos tener un archivo de configuración para estos valores?
#define USERNAME_MAX_LEN 32
#define PASSWORD_MAX_LEN 32
#define MAX_USERS 500
#define MONITOR_BANNER "SOCKS-MONITOR"
#define MONITOR_VERSION "1.0"
#define INVALID_CREDENTIALS_MSG "ERR Invalid credentials\n"
#define INVALID_CREDENTIALS_MSG_LEN 24
#define INVALID_CREDENTIALS_FORMAT_MSG "ERR Invalid format. Use username:password\n"
#define INVALID_CREDENTIALS_FORMAT_MSG_LEN 51
#define MAX_CONFIG_PARAM_LEN 64
#define INITIAL_USER_LIST_CAPACITY 1463

static const struct fd_handler monitor_handler = {
    .handle_read = monitor_read,
    .handle_write = monitor_write,
    .handle_close = monitor_close,
};

int parse_username_password(const char *input, char *username, char *password) {
    const char *delimiter = strchr(input, ':');
    if (delimiter == NULL) {
        return 0;
    }

    size_t username_len = delimiter - input;
    size_t password_len = strlen(delimiter + 1);

    if (username_len == 0 || password_len == 0) {
        return 0;
    }

    strncpy(username, input, username_len);
    username[username_len] = '\0';

    strncpy(password, delimiter + 1, password_len);
    password[password_len] = '\0';

    return 1;
}

void monitor_accept_connection(struct selector_key *key) {
    int server_fd = key->fd;
    fd_selector selector = key->data;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Accept a new connection
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if(client_fd < 0) {
        log(ERROR, "monitor accept() failed: %s", strerror(errno));
        return;
    }

    // Set the client socket to non-blocking
    if(selector_fd_set_nio(client_fd) == -1) {
        log(ERROR, "Failed to set monitor client socket to non-blocking: %s", strerror(errno));
        close(client_fd);
        return;
    }

    // Monitor connection structure
    monitor_connection *new_connection = malloc(sizeof(monitor_connection));
    if(new_connection == NULL) {
        log(ERROR, "Failed to allocate memory for new monitor connection");
        close(client_fd);
        return;
    }
    
    buffer_init(&new_connection->read_buffer, MONITOR_BUFFER_SIZE, new_connection->raw_read);
    buffer_init(&new_connection->write_buffer, MONITOR_BUFFER_SIZE, new_connection->raw_write);
    memset(new_connection->command, 0, MAX_COMMAND_SIZE);
    memset(new_connection->response, 0, MAX_RESPONSE_SIZE);
    new_connection->client_fd = client_fd;
    new_connection->handshake_done = 0;
    new_connection->state = MONITOR_HANDSHAKE_WRITE;

    const char *banner = "Please enter Username:password (in that format)\n";
    size_t banner_len = strlen(banner);
    for(size_t i = 0; i < banner_len; i++) {
        buffer_write(&new_connection->write_buffer, banner[i]);
    }

    if(selector_register(selector, client_fd, &monitor_handler, OP_WRITE, new_connection) != SELECTOR_SUCCESS) {
        log(ERROR, "selector_register() failed for monitor client");
        close(client_fd);
        free(new_connection);
        return;
    }
    log(INFO, "New monitor client connected: fd=%d", client_fd);
}

void monitor_read(struct selector_key *key) {
    monitor_connection *connection = key->data;
    if(connection == NULL) {
        log(ERROR, "Monitor connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return;
    }

    switch(connection->state) {
        case MONITOR_HANDSHAKE_READ:
            if(handle_monitor_handshake_read(&connection->read_buffer, key->fd) == 0) {
                connection->state = MONITOR_COMMAND_READ;
                selector_set_interest_key(key, OP_READ);
            }
            break;
        case MONITOR_COMMAND_READ:
            if(handle_monitor_command_read(&connection->read_buffer, key->fd, &connection->write_buffer, connection) != 0) {
                connection->state = MONITOR_ERROR_STATE;
                selector_set_interest_key(key, OP_NOOP);
                return;
            }
            if(connection->state == MONITOR_COMMAND_WRITE) {
                selector_set_interest_key(key, OP_WRITE);
            }
            break;
        default:
            log(ERROR, "Unknown state in monitor_read: %d", connection->state);
            connection->state = MONITOR_ERROR_STATE;
            selector_set_interest_key(key, OP_NOOP);
            break;
    }
}

void monitor_write(struct selector_key *key) {
    monitor_connection *connection = key->data;
    if(connection == NULL) {
        log(ERROR, "Monitor connection data is NULL");
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
        return;
    }

    switch(connection->state) {
        case MONITOR_HANDSHAKE_WRITE:
            if(handle_monitor_handshake_write(&connection->write_buffer, key->fd) != 0) {
                connection->state = MONITOR_ERROR_STATE;
                selector_set_interest_key(key, OP_NOOP);
                return;
            }
            if(!buffer_can_read(&connection->write_buffer)) {
                connection->state = MONITOR_HANDSHAKE_READ;
                selector_set_interest_key(key, OP_READ);
            }
            break;
        case MONITOR_COMMAND_WRITE:
            if(handle_monitor_command_write(&connection->write_buffer, key->fd) != 0) {
                connection->state = MONITOR_ERROR_STATE;
                selector_set_interest_key(key, OP_NOOP);
                return;
            }
            if(!buffer_can_read(&connection->write_buffer)) {
                connection->state = MONITOR_COMMAND_READ;
                selector_set_interest_key(key, OP_READ);
            }
            break;
        default:
            log(ERROR, "Unexpected state in monitor_write: %d", connection->state);
            selector_set_interest_key(key, OP_NOOP);
            break;
    }
}

void monitor_close(struct selector_key *key) {
    if(key->data != NULL) {
        free(key->data);
    }
    close(key->fd);
    log(INFO, "Monitor client disconnected: fd=%d", key->fd);
}

int handle_monitor_handshake_read(struct buffer *read_buff, int fd) {
    size_t n;
    uint8_t *ptr = buffer_write_ptr(read_buff, &n);

    ssize_t read = recv(fd, ptr, n, 0);
    if(read < 0) {
        log(ERROR, "monitor recv() failed: %s", strerror(errno));
        return -1;
    }
    buffer_write_adv(read_buff, read);

    while(buffer_can_read(read_buff)) {
        char line[64];
        int line_len = 0;
        int found_newline = 0;
        while(buffer_can_read(read_buff) && line_len < 63) {
            char c = buffer_read(read_buff);
            if(c == '\n') {
                line[line_len] = '\0';
                found_newline = 1;
                break;
            }
            line[line_len++] = c;
        }
        if(!found_newline) {
            break;
        }

        char username[USERNAME_MAX_LEN], password[PASSWORD_MAX_LEN];
        if(parse_username_password(line, username, password)) {
            buffer_reset(read_buff);
            if (credentials_are_valid(username, password) == 0) {
                snprintf((char *) read_buff->data, read_buff->limit - read_buff->data, "OK Welcome %s\n", username);
                buffer_write_adv(read_buff, strlen((char *) read_buff->data));
                ssize_t sent = send(fd, read_buff->data, strlen((char *) read_buff->data), 0);
                buffer_reset(read_buff);
                if (sent < 0) {
                    log(ERROR, "monitor send() failed: %s", strerror(errno));
                    return -1;
                }
                log(INFO, "monitor: successful authentication for user '%s'", username);
                return 0;
            } else {
                ssize_t sent = send(fd, INVALID_CREDENTIALS_MSG, strlen(INVALID_CREDENTIALS_MSG), 0);
                if (sent < 0) {
                    log(ERROR, "monitor: failed to send error message: %s", strerror(errno));
                }
                log(ERROR, "monitor: invalid credentials from user '%s', closing connection", username);
                return -1;
            }
        } else {
            log(INFO, "monitor: invalid username:password format: '%s'", line);
            ssize_t sent = send(fd, INVALID_CREDENTIALS_FORMAT_MSG, strlen(INVALID_CREDENTIALS_FORMAT_MSG), 0);
            if (sent < 0) {
                log(ERROR, "monitor: failed to send error message: %s", strerror(errno));
            }
            return -1;
        }
    }
    return 1;
}

int handle_monitor_handshake_write(struct buffer *write_buff, int fd) {
    size_t n;
    uint8_t *ptr = buffer_read_ptr(write_buff, &n);
    ssize_t sent = send(fd, ptr, n, 0);
    if(sent < 0) {
        log(ERROR, "monitor send() failed: %s", strerror(errno));
        return -1;
    }
    buffer_read_adv(write_buff, sent);
    return 0;
}

int handle_monitor_command_read(struct buffer *read_buff, int fd, struct buffer *write_buff, monitor_connection *conn) {
    size_t n;
    uint8_t *ptr = buffer_write_ptr(read_buff, &n);

    ssize_t read = recv(fd, ptr, n, 0);
    if(read < 0) {
        log(ERROR, "monitor command recv() failed: %s", strerror(errno));
        return -1;
    }
    buffer_write_adv(read_buff, read);

    if(buffer_can_read(read_buff)) {
        char line[256];
        int line_len = 0;
        
        while(buffer_can_read(read_buff) && line_len < 255) {
            char c = buffer_read(read_buff);
            if(c == '\n') {
                line[line_len] = '\0';
                break;
            }
            line[line_len++] = c;
        }
        
        if(line_len > 0) {
            strncpy(conn->command, line, MAX_COMMAND_SIZE - 1);
            conn->command[MAX_COMMAND_SIZE - 1] = '\0';
            
            char *space = strchr(conn->command, ' ');
            if(space != NULL) {
                *space = '\0';
                char *args = space + 1;
                
                if(strcmp(conn->command, "STATS") == 0) {
                    handle_stats_command(conn);
                } else if(strcmp(conn->command, "CONNECTIONS") == 0) {
                    handle_connections_command(conn);
                } else if(strcmp(conn->command, "USERS") == 0) {
                    handle_users_command(conn);
                } else if(strcmp(conn->command, "CONFIG") == 0) {
                    handle_config_command(conn, args);
                } else if(strcmp(conn->command, "ACCESS_LOG") == 0) {
                    handle_access_log_user_command(conn, args);
                } else if(strcmp(conn->command, "QUIT") == 0) {
                    log(INFO, "I'm here");
                    snprintf(conn->response, MAX_RESPONSE_SIZE, "OK Goodbye\n");
                    buffer_reset(write_buff);
                    size_t response_len = strlen(conn->response);
                    for(size_t i = 0; i < response_len; i++) {
                        buffer_write(write_buff, conn->response[i]);
                    }
                    return -1;
                } else {
                    snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Unknown command: %s\n", conn->command);
                }
            } else {
                if(strcmp(conn->command, "STATS") == 0) {
                    handle_stats_command(conn);
                } else if(strcmp(conn->command, "CONNECTIONS") == 0) {
                    handle_connections_command(conn);
                } else if(strcmp(conn->command, "USERS") == 0) {
                    handle_users_command(conn);
                } else if(strcmp(conn->command, "ACCESS_LOG") == 0) {
                    handle_access_log_command(conn);
                }else if(strcmp(conn->command, "QUIT") == 0) {
                    log(INFO, "I'm not there");
                    snprintf(conn->response, MAX_RESPONSE_SIZE, "OK Goodbye\n");
                    send(fd, conn->response, strlen(conn->response), 0);
                    close(conn->client_fd);
                } else {
                    snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Unknown command: %s\n", conn->command);
                }        
                size_t response_len = strlen(conn->response);
                for(size_t i = 0; i < response_len; i++) {
                    buffer_write(write_buff, conn->response[i]);
                }
            }
            
            buffer_reset(write_buff);
            size_t response_len = strlen(conn->response);
            for(size_t i = 0; i < response_len; i++) {
                buffer_write(write_buff, conn->response[i]);
            }
            
            conn->state = MONITOR_COMMAND_WRITE;
        }
    }
    return 0;
}

int handle_monitor_command_write(struct buffer *write_buff, int fd) {
    size_t n;
    uint8_t *ptr = buffer_read_ptr(write_buff, &n);
    ssize_t sent = send(fd, ptr, n, 0);
    if(sent < 0) {
        log(ERROR, "monitor command send() failed: %s", strerror(errno));
        return -1;
    }
    buffer_read_adv(write_buff, sent);
    return 0;
}

void handle_stats_command(monitor_connection *conn) {
    uint64_t total_conn = metrics_get_total_connections();
    uint64_t current_conn = metrics_get_current_connections();
    uint64_t max_conn = metrics_get_max_concurrent_connections();
    uint64_t total_bytes = metrics_get_total_bytes_transferred();
    int user_count = metrics_get_user_count();
    time_t uptime = metrics_get_server_uptime();
    
    snprintf(conn->response, MAX_RESPONSE_SIZE, 
             "OK\nall time connections:%lu\nbytes transferred:%lu\nactive users:%d\ncurrent connections:%lu\nmax concurrent connections:%lu\nserver uptime:%lds\n",
             total_conn, total_bytes, user_count, current_conn, max_conn, uptime);
}

void handle_connections_command(monitor_connection *conn) {
    uint64_t current_conn = metrics_get_current_connections();
    uint64_t max_conn = metrics_get_max_concurrent_connections();
    
    snprintf(conn->response, MAX_RESPONSE_SIZE, 
             "OK\ncurrent connections:%lu\nmax concurrent connections:%lu\n",
             current_conn, max_conn);
}

static char *build_user_list_string(user_info *users, int count, monitor_connection *conn) {
    int capacity = INITIAL_USER_LIST_CAPACITY;
    char *user_list = malloc(capacity);
    if (user_list == NULL) {
        log(ERROR, "Failed to allocate memory for user list");
        snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Memory allocation failed\n");
        return NULL;
    }

    strcpy(user_list, "OK ");
    int pos = 3;

    for (int i = 0; i < count; i++) {
        int written = snprintf(NULL, 0, "%s ", users[i].username);
        if (pos + written >= capacity) {
            capacity += INITIAL_USER_LIST_CAPACITY;
            char *new_user_list = realloc(user_list, capacity);
            if (new_user_list == NULL) {
                log(ERROR, "Failed to reallocate memory for user list");
                free(user_list);
                snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Memory allocation failed\n");
                return NULL;
            }
            user_list = new_user_list;
        }

        snprintf(user_list + pos, capacity - pos, "%s ", users[i].username);
        log(INFO, "User %d: %s", i, users[i].username);
        pos += written;
    }

    if (pos > 3) {
        user_list[pos - 1] = '\n';
        user_list[pos] = '\0';
    }

    return user_list;
}

void handle_users_command(monitor_connection *conn) {
    user_info users[MAX_USERS];
    int count = metrics_get_user_count();
    metrics_get_users(users, MAX_USERS);
    
    if (count == 0) {
        snprintf(conn->response, MAX_RESPONSE_SIZE, "OK No active users\n");
        return;
    }
    
    char *user_list = build_user_list_string(users, count, conn);

    strncpy(conn->response, user_list, MAX_RESPONSE_SIZE - 1);
    conn->response[MAX_RESPONSE_SIZE - 1] = '\0';

    log(INFO, "Active users: %d", count);
    log(INFO, "User list: %s", user_list);

    strncpy(conn->response, user_list, MAX_RESPONSE_SIZE - 1);
    conn->response[MAX_RESPONSE_SIZE - 1] = '\0';
    free(user_list);
}

void handle_config_command(monitor_connection *conn, const char *args) {
    char param[MAX_CONFIG_PARAM_LEN], value[MAX_CONFIG_PARAM_LEN];
    if (sscanf(args, "%63s %63s", param, value) == 2) {
        if (strcmp(param, "timeout") == 0) {
            int timeout = atoi(value);
            if (timeout > 0) {
                change_timeout(timeout);
                snprintf(conn->response, MAX_RESPONSE_SIZE, "OK timeout set to %d\n", timeout);
            } else {
                snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Invalid timeout value\n");
            }
        } else {
            snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Unknown config parameter: %s\n", param);
        }
    } else {
        snprintf(conn->response, MAX_RESPONSE_SIZE, "ERR Usage: CONFIG <param> <value>\n");
    }
}

void handle_access_log_command(monitor_connection *conn) {
    user_info users[MAX_USERS];
    int count = metrics_get_all_time_user_count();
    metrics_get_all_time_users(users, MAX_USERS);
    log(INFO, "Amount of all time users: %d", count);
    for (int  i=0; i<count; i++) {
        log(INFO, "User %d: %s", i, users[i].username);
        log(INFO, "Last seen: %ld", users[i].last_seen);
        log(INFO, "Sites visited: %d", users[i].site_count);
    }
    buffer_reset(&conn->write_buffer);
    int written = snprintf(conn->response, MAX_RESPONSE_SIZE, "OK Access log for all users:\n");
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < users[i].site_count; j++) {
            site_visit *sv = &users[i].sites_visited[j];
            char timebuf[32];
            struct tm *tm_info = localtime(&sv->timestamp);
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
            written += snprintf(conn->response + written, MAX_RESPONSE_SIZE - written,
                "%s: %s:%s [%s] %s\n",
                users[i].username,
                sv->destination_host,
                sv->destination_port,
                timebuf,
                sv->success ? "SUCCESS" : "FAIL");
                if (written >= MAX_RESPONSE_SIZE - 128) break;
            }
            if (written >= MAX_RESPONSE_SIZE - 128) break;
        }
        if (written == 0) {
            snprintf(conn->response, MAX_RESPONSE_SIZE, "OK No access log entries\n");
        }
}

void handle_access_log_user_command(monitor_connection *conn, const char *username) {
    user_info users[MAX_USERS];
    int count = metrics_get_all_time_user_count();
    metrics_get_all_time_users(users, MAX_USERS);
    int found = 0;
    buffer_reset(&conn->write_buffer);
    int written = snprintf(conn->response, MAX_RESPONSE_SIZE, "OK Access log for user %s:\n", username);
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            found = 1;
            for (int j = 0; j < users[i].site_count; j++) {
                site_visit *sv = &users[i].sites_visited[j];
                char timebuf[32];
                struct tm *tm_info = localtime(&sv->timestamp);
                strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
                written += snprintf(conn->response + written, MAX_RESPONSE_SIZE - written,
                    "%s:%s [%s] %s\n",
                    sv->destination_host,
                    sv->destination_port,
                    timebuf,
                    sv->success ? "SUCCESS" : "FAIL");
                if (written >= MAX_RESPONSE_SIZE - 128) break;
            }
            break;
        }
    }
    if (!found) {
        snprintf(conn->response, MAX_RESPONSE_SIZE, "OK No access log entries for user %s\n", username);
    }
} 