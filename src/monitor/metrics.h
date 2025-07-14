#ifndef METRICS_H_
#define METRICS_H_

#include "../server/include/selector.h"
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "../utils/logger.h"

#define MAX_USERNAME_LEN 64
#define MAX_IP_LEN 46
#define MAX_HOSTNAME_LEN 256
#define MAX_PORT_LEN 6
#define MAX_METHOD_LEN 16

//@todo: ver si hace falta agregar más data
typedef struct {
    char * destination_host;
    char * destination_port;
    char * method;
    time_t timestamp;
    bool success;
}site_visit;
typedef struct {
    char username[MAX_USERNAME_LEN];
    char ip_address[MAX_IP_LEN];
    time_t last_seen;
    site_visit * sites_visited;
    int site_count;
} user_info;

void metrics_init(struct selector_init *conf);
void metrics_free();

int metrics_connection_start();
void metrics_connection_end();
void metrics_bytes_transferred(ssize_t bytes);
void metrics_add_user_site(const char *username, char *site, int success, char * destination_port);

void metrics_add_user(const char *username, const char *ip_address);
void metrics_remove_user(const char *username);

uint64_t metrics_get_total_connections();
uint64_t metrics_get_current_connections();
uint64_t metrics_get_max_concurrent_connections();
uint64_t metrics_get_total_bytes_transferred();
time_t metrics_get_server_uptime();
int metrics_get_user_count();
void metrics_get_users(user_info *users, int max_users);

int metrics_get_timeout();
void metrics_set_timeout(int seconds);
int metrics_get_max_connections();

int metrics_get_all_time_user_count();
void metrics_get_all_time_users(user_info *users, int max_users);

#endif 