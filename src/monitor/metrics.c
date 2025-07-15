#include "metrics.h"

#include "../server/include/selector.h"

#define MAX_CONNECTIONS 511
#define INITIAL_SITES_VECTOR_CAPACITY 10
#define MAX_ALL_TIME_USERS 1000

static pthread_mutex_t metrics_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t total_connections = 0;
static uint64_t current_connections = 0;
static uint64_t max_concurrent_connections = 0;
static uint64_t total_bytes_transferred = 0;
static time_t server_start_time = 0;
static struct selector_init *config;

static user_info active_users[MAX_CONNECTIONS];
static int active_user_count = 0;

static user_info all_time_users[MAX_ALL_TIME_USERS];
static int all_time_user_count = 0;

void change_timeout(int seconds) {
    pthread_mutex_lock(&metrics_mutex);
    update_connect_timeout(seconds * 1000000);
    pthread_mutex_unlock(&metrics_mutex);
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

void metrics_init(struct selector_init *conf) {
    config = conf;
    server_start_time = time(NULL);
    memset(active_users, 0, sizeof(active_users));
    memset(all_time_users, 0, sizeof(all_time_users));
    active_user_count = 0;
}

int metrics_connection_start() {
    pthread_mutex_lock(&metrics_mutex);
    if (current_connections >= MAX_CONNECTIONS) {
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

void metrics_bytes_transferred(ssize_t bytes) {
    pthread_mutex_lock(&metrics_mutex);
    total_bytes_transferred += bytes;
    pthread_mutex_unlock(&metrics_mutex);
}

static void find_and_update_users(user_info * users, int *current_count, char * username) {
    for (int i = 0; i < *current_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].last_seen = time(NULL);
            return;
        }
    }
    if (*current_count < MAX_CONNECTIONS) {
        strncpy(users[*current_count].username, username, sizeof(users[*current_count].username) - 1);
        users[*current_count].last_seen = time(NULL);
        (*current_count)++;
    }
}

void metrics_add_user(const char *username) {
    pthread_mutex_lock(&metrics_mutex);
    
    if (active_user_count < MAX_CONNECTIONS) {
        find_and_update_users(all_time_users, &all_time_user_count, (char *)username);
        find_and_update_users(active_users, &active_user_count, (char *)username);
    }
    pthread_mutex_unlock(&metrics_mutex);
}

void metrics_remove_user(const char *username) {
    pthread_mutex_lock(&metrics_mutex);
    
    for (int i = 0; i < active_user_count; i++) {
        if (strcmp(active_users[i].username, username) == 0) {
            for (int j = i; j < active_user_count - 1; j++) {
                active_users[j] = active_users[j + 1];
            }
            active_user_count--;
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
    int result = active_user_count;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

void metrics_get_users(user_info *users, int max_users) {
    pthread_mutex_lock(&metrics_mutex);
    int count = (active_user_count < max_users) ? active_user_count : max_users;
    memcpy(users, active_users, count * sizeof(user_info));
    pthread_mutex_unlock(&metrics_mutex);
}

int metrics_get_max_connections() {
    pthread_mutex_lock(&metrics_mutex);
    int current_MAX_CONNECTIONS = MAX_CONNECTIONS;
    pthread_mutex_unlock(&metrics_mutex);
    return current_MAX_CONNECTIONS;
}

//@todo: ver si cambiar metodo de guardar usuarios activos por el hashmap
void metrics_add_user_site(const char *username, char *site, int success, char* destination_port) {
    pthread_mutex_lock(&metrics_mutex);
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
    pthread_mutex_unlock(&metrics_mutex);
}

int metrics_get_all_time_user_count() {
    pthread_mutex_lock(&metrics_mutex);
    int result = all_time_user_count;
    pthread_mutex_unlock(&metrics_mutex);
    return result;
}

void metrics_get_all_time_users(user_info *users, int max_users) {
    pthread_mutex_lock(&metrics_mutex);
    int count = (all_time_user_count < max_users) ? all_time_user_count : max_users;
    memcpy(users, all_time_users, count * sizeof(user_info));
    pthread_mutex_unlock(&metrics_mutex);
}

//@todo: ver si borrar
// static void free_site_visit(site_visit *site) {
//     if (site != NULL) {
//         //free(site->destination_host);
//         //free(site->destination_port);
//         free(site);
//     }
// }

void metrics_free() {
    // pthread_mutex_lock(&metrics_mutex);
    // for (int i = 0; i < all_time_user_count; i++) {
    //     for (int i=0; i < all_time_users[i].site_count; i++) {
    //         free_site_visit(&all_time_users[i].sites_visited[i]);
    //     }
    // }
    // pthread_mutex_unlock(&metrics_mutex);
    pthread_mutex_destroy(&metrics_mutex);
}