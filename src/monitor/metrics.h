#ifndef METRICS_H_
#define METRICS_H_

#include <stdint.h>
#include <time.h>

#define MAX_USERNAME_LEN 64
#define MAX_IP_LEN 46

//@todo: ver si hace falta agregar más data
typedef struct {
    char username[MAX_USERNAME_LEN];
    char ip_address[MAX_IP_LEN];
    time_t last_seen;
} user_info;

void metrics_init();

void metrics_connection_start();
void metrics_connection_end();
void metrics_bytes_transferred(uint64_t bytes);

void metrics_add_user(const char *username, const char *ip_address);
void metrics_remove_user(const char *username);

uint64_t metrics_get_total_connections();
uint64_t metrics_get_current_connections();
uint64_t metrics_get_max_concurrent_connections();
uint64_t metrics_get_total_bytes_transferred();
time_t metrics_get_server_uptime();
int metrics_get_user_count();
void metrics_get_users(user_info *users, int max_users);

//@todo: falta poder setear variables?
int metrics_get_timeout();
void metrics_set_timeout(int seconds);
int metrics_get_max_connections();
void metrics_set_max_connections(int max);

#endif 