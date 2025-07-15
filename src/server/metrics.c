#include "include/metrics.h"

#define MAX_CONNECTIONS 511
#define INITIAL_SITES_VECTOR_CAPACITY 10
#define MAX_ALL_TIME_USERS 1000
#define ALL_TIME_USERS_INITIAL_CAPACITY 100

static uint64_t total_connections = 0;
static uint64_t current_connections = 0;
static uint64_t max_concurrent_connections = 0;
static uint64_t total_bytes_transferred = 0;
static time_t server_start_time = 0;
static struct selector_init *config;

static user_info active_users[MAX_CONNECTIONS];
static int active_user_count = 0;


static user_info *all_time_users = NULL;
static int all_time_user_count = 0;
static int all_time_user_capacity = 0;

void change_timeout(int seconds) {
    update_connect_timeout(seconds * 1000000);
}

static void add_user_site(user_info *user_info, int * site_count, bool success, char * destination_port, char *site) {
    if (site == NULL || user_info == NULL) {
        return;
    }
    if (*site_count % INITIAL_SITES_VECTOR_CAPACITY == 0) {
        site_visit *new_sites = realloc(user_info->sites_visited, sizeof(site_visit) * (*site_count + INITIAL_SITES_VECTOR_CAPACITY));
        if (new_sites == NULL) {
            log(ERROR, "Failed to reallocate memory for user sites visited");
            free(user_info->sites_visited);
            return;
        }
        user_info->sites_visited = new_sites;
    }
    user_info->sites_visited[*site_count].destination_host = site;
    user_info->sites_visited[*site_count].destination_port = destination_port;
    user_info->sites_visited[*site_count].timestamp = time(NULL);
    user_info->sites_visited[*site_count].success = success;
    (*site_count)++;
    return;
}

static void find_and_update_active_users(const char *username) {
    for (int i = 0; i < active_user_count; i++) {
        if (strcmp(active_users[i].username, username) == 0) {
            active_users[i].last_seen = time(NULL);
            return;
        }
    }
    if (active_user_count < MAX_CONNECTIONS) {
        strncpy(active_users[active_user_count].username, username, sizeof(active_users[active_user_count].username) - 1);
        active_users[active_user_count].last_seen = time(NULL);
        active_user_count++;
    }
}

static void find_and_update_all_time_users(const char *username) {
    for (int i = 0; i < all_time_user_count; i++) {
        if (strcmp(all_time_users[i].username, username) == 0) {
            all_time_users[i].last_seen = time(NULL);
            return;
        }
    }
    if (all_time_user_count >= all_time_user_capacity) {
        int new_capacity = (all_time_user_capacity > 0) ? all_time_user_capacity * 2 : ALL_TIME_USERS_INITIAL_CAPACITY;
        user_info *new_array = realloc(all_time_users, sizeof(user_info) * new_capacity);
        if (new_array == NULL) {
            return;
        }
        all_time_users = new_array;
        all_time_user_capacity = new_capacity;
    }
    memset(&all_time_users[all_time_user_count], 0, sizeof(user_info));
    strncpy(all_time_users[all_time_user_count].username, username, sizeof(all_time_users[all_time_user_count].username) - 1);
    all_time_users[all_time_user_count].last_seen = time(NULL);
    all_time_user_count++;
}

void metrics_init(struct selector_init *conf) {
    config = conf;
    server_start_time = time(NULL);
    memset(active_users, 0, sizeof(active_users));
    active_user_count = 0;
    all_time_user_capacity = ALL_TIME_USERS_INITIAL_CAPACITY;
    all_time_users = malloc(sizeof(user_info) * all_time_user_capacity);
    all_time_user_count = 0;
}

int metrics_connection_start() {
    if (current_connections >= MAX_CONNECTIONS) {
        return 1;
    }
    total_connections++;
    current_connections++;
    if (current_connections > max_concurrent_connections) {
        max_concurrent_connections = current_connections;
    }
    return 0;
}

void metrics_connection_end() {
    if (current_connections > 0) {
        current_connections--;
    }
}

void metrics_bytes_transferred(ssize_t bytes) {
    total_bytes_transferred += bytes;
}

void metrics_add_user(const char *username) {
    find_and_update_active_users(username);
    find_and_update_all_time_users(username);
}

void metrics_remove_user(const char *username) {
    
    for (int i = 0; i < active_user_count; i++) {
        if (strcmp(active_users[i].username, username) == 0) {
            for (int j = i; j < active_user_count - 1; j++) {
                active_users[j] = active_users[j + 1];
            }
            active_user_count--;
            break;
        }
    }
}

uint64_t metrics_get_total_connections() {
    uint64_t result = total_connections;
    return result;
}

uint64_t metrics_get_current_connections() {
    uint64_t result = current_connections;
    return result;
}

uint64_t metrics_get_max_concurrent_connections() {
    uint64_t result = max_concurrent_connections;
    return result;
}

uint64_t metrics_get_total_bytes_transferred() {
    uint64_t result = total_bytes_transferred;
    return result;
}

time_t metrics_get_server_uptime() {
    return time(NULL) - server_start_time;
}

int metrics_get_user_count() {
    int result = active_user_count;
    return result;
}

void metrics_get_users(user_info *users, int max_users) {
    int count = (active_user_count < max_users) ? active_user_count : max_users;
    memcpy(users, active_users, count * sizeof(user_info));
}

int metrics_get_max_connections() {
    int current_MAX_CONNECTIONS = MAX_CONNECTIONS;
    return current_MAX_CONNECTIONS;
}

void metrics_add_user_site(const char *username, char *site, int success, char* destination_port) {
    for (int i = 0; i < active_user_count; i++) {
        if (strcmp(active_users[i].username, username) == 0) {
            add_user_site(&active_users[i], &active_users[i].site_count, success, destination_port, site);
            break;
        }
    }
    for (int i = 0; i < all_time_user_count; i++) {
        if (strcmp(all_time_users[i].username, username) == 0) {
            add_user_site(&all_time_users[i], &all_time_users[i].site_count, success, destination_port, site);
            break;
        }
    }
}

int metrics_get_all_time_user_count() {
    int result = all_time_user_count;
    return result;
}

void metrics_get_all_time_users(user_info *users, int max_users) {
    int count = (all_time_user_count < max_users) ? all_time_user_count : max_users;
    memcpy(users, all_time_users, count * sizeof(user_info));
}


void metrics_free() {
    if (all_time_users != NULL) {
        free(all_time_users);
    }
}