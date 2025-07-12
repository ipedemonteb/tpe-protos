#include <pthread.h>
#include <string.h>
#include <time.h>
#include "metrics.h"

#define MAX_USERS 100

static pthread_mutex_t metrics_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t total_connections = 0;
static uint64_t current_connections = 0;
static uint64_t max_concurrent_connections = 0;
static uint64_t total_bytes_transferred = 0;
static time_t server_start_time = 0;
static struct selector_init *config;

static user_info active_users[MAX_USERS];
static int user_count = 0;

static int timeout_seconds = 30;
static int max_connections = 1;

void metrics_init(struct selector_init *conf) {
    config = conf;
    server_start_time = time(NULL);
    memset(active_users, 0, sizeof(active_users));
    user_count = 0;
}

int metrics_connection_start() {
    pthread_mutex_lock(&metrics_mutex);
    if (current_connections >= max_connections) {
        return 1;
    }
    total_connections++;
    current_connections++;
    if (current_connections > max_concurrent_connections) {
        max_concurrent_connections = current_connections;
    }
    pthread_mutex_unlock(&metrics_mutex);
    return 0;
}

void metrics_connection_end() {
    pthread_mutex_lock(&metrics_mutex);
    if (current_connections > 0) {
        current_connections--;
    }
    pthread_mutex_unlock(&metrics_mutex);
}

void metrics_bytes_transferred(uint64_t bytes) {
    pthread_mutex_lock(&metrics_mutex);
    total_bytes_transferred += bytes;
    pthread_mutex_unlock(&metrics_mutex);
}

void metrics_add_user(const char *username, const char *ip_address) {
    pthread_mutex_lock(&metrics_mutex);
    
    if (user_count < MAX_USERS) {
        for (int i = 0; i < user_count; i++) {
            if (strcmp(active_users[i].username, username) == 0) {
                strncpy(active_users[i].ip_address, ip_address, sizeof(active_users[i].ip_address) - 1);
                active_users[i].last_seen = time(NULL);
                pthread_mutex_unlock(&metrics_mutex);
                return;
            }
        }
    
        strncpy(active_users[user_count].username, username, sizeof(active_users[user_count].username) - 1);
        strncpy(active_users[user_count].ip_address, ip_address, sizeof(active_users[user_count].ip_address) - 1);
        active_users[user_count].last_seen = time(NULL);
        user_count++;
    }
    
    pthread_mutex_unlock(&metrics_mutex);
}

void metrics_remove_user(const char *username) {
    pthread_mutex_lock(&metrics_mutex);
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(active_users[i].username, username) == 0) {
            for (int j = i; j < user_count - 1; j++) {
                active_users[j] = active_users[j + 1];
            }
            user_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&metrics_mutex);
}

uint64_t metrics_get_total_connections() {
    pthread_mutex_lock(&metrics_mutex);
    uint64_t result = total_connections;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

uint64_t metrics_get_current_connections() {
    pthread_mutex_lock(&metrics_mutex);
    uint64_t result = current_connections;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

uint64_t metrics_get_max_concurrent_connections() {
    pthread_mutex_lock(&metrics_mutex);
    uint64_t result = max_concurrent_connections;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

uint64_t metrics_get_total_bytes_transferred() {
    pthread_mutex_lock(&metrics_mutex);
    uint64_t result = total_bytes_transferred;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

time_t metrics_get_server_uptime() {
    return time(NULL) - server_start_time;
}

int metrics_get_user_count() {
    pthread_mutex_lock(&metrics_mutex);
    int result = user_count;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

void metrics_get_users(user_info *users, int max_users) {
    pthread_mutex_lock(&metrics_mutex);
    int count = (user_count < max_users) ? user_count : max_users;
    memcpy(users, active_users, count * sizeof(user_info));
    pthread_mutex_unlock(&metrics_mutex);
}

int metrics_get_timeout() {
    pthread_mutex_lock(&metrics_mutex);
    int timeout = (int) config->select_timeout.tv_sec;
    pthread_mutex_unlock(&metrics_mutex);
    return timeout;
}

void metrics_set_timeout(int seconds) {
    pthread_mutex_lock(&metrics_mutex);
    config->select_timeout.tv_sec = seconds;
    pthread_mutex_unlock(&metrics_mutex);
}

int metrics_get_max_connections() {
    pthread_mutex_lock(&metrics_mutex);
    int current_max_connections = max_connections;
    pthread_mutex_unlock(&metrics_mutex);
    return current_max_connections;
}

void metrics_set_max_connections(int max) {
    pthread_mutex_lock(&metrics_mutex);
    max_connections = max;
    pthread_mutex_unlock(&metrics_mutex);
} 