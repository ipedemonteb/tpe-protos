#ifndef __HASHMAP_H__
#define __HASHMAP_H__

#include <stddef.h>

//porque el protocolo socks5 da 8bits para definir el largo del user/pass
#define ENTRY_LENGTH 255 

typedef struct static_entry {
    char key[ENTRY_LENGTH];
    char value[ENTRY_LENGTH];
    int present
} static_entry;

typedef struct hashmap {
    static_entry *entries;
    size_t size;
    size_t capacity;
} hashmap_t;

hashmap_t *create_hashmap(size_t capacity);

void destroy_hashmap(hashmap_t *map);

int hashmap_insert(hashmap_t *map, const char *key, const char *value);

const static_entry *hashmap_get(const hashmap_t *map, const char *key);

#endif