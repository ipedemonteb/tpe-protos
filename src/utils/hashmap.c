#include "hashmap.h"

// djb2
static unsigned long _hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}

hashmap_t *rehash(hashmap_t *map, size_t new_capacity)
{
    if (new_capacity <= map->capacity)
        return map;
    static_entry *new_entries = malloc(sizeof(static_entry) * new_capacity);
    if (!new_entries)
        return NULL;
    for (size_t i = 0; i < map->capacity; i++)
    {
        if (map->entries[i].present)
        {
            unsigned long hash = _hash(map->entries[i].key);
            int position = hash % new_capacity;
            while (new_entries[position].present && position < ((int)new_capacity - 1))
                position++;
            new_entries[position] = map->entries[i];
        }
    }

    free(map->entries);
    map->entries = new_entries;
    map->capacity = new_capacity;
    return map;
}

hashmap_t *create_hashmap(size_t capacity)
{
    hashmap_t *map = malloc(sizeof(hashmap_t));
    if (!map)
    {
        return NULL;
    }
    map->entries = malloc(sizeof(static_entry) * capacity);
    if (!map->entries)
    {
        free(map);
        return NULL;
    }
    map->size = 0;
    map->capacity = capacity;
    return map;
}

void destroy_hashmap(hashmap_t *map)
{
    if (map)
    {
        if (map->entries)
        {
            free(map->entries);
        }
        free(map);
    }
}

int hashmap_insert(hashmap_t *map, const char *key, const char *value)
{
    unsigned long hash = _hash(key);
    int position = hash % map->capacity;
    while (map->entries[position].present && position < ((int)map->capacity - 1))
        position++;
    if (!map->entries[position].present)
    {
        strncpy(map->entries[position].key, key, ENTRY_LENGTH);
        strncpy(map->entries[position].value, value, ENTRY_LENGTH);
        map->entries[position].present = 1;
        map->size++;
        return 1;
    }
    rehash(map, map->capacity * 2);
    hashmap_insert(map, key, value);
    return 0;
}

const static_entry *hashmap_get(const hashmap_t *map, const char *key)
{
    unsigned long hash = _hash(key);
    int position = hash % map->capacity;
    while (map->entries[position].present && position < ((int)map->capacity - 1) && strcmp(key, map->entries[position].key))
    {
        position++;
    }
    if (map->entries[position].present && !strcmp(key, map->entries[position].key))
    {
        return &(map->entries[position]);
    }
    return NULL;
}
