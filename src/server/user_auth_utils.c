#include "user_auth_utils.h"
#include <stdint.h>
#include "../utils/hashmap.h"
#include <stdlib.h>
#include <stdio.h>


#define HASHMAP_SIZE 32

hashmap_t *user_credentials = NULL;

static int _fetch_user_credentials()
{
    user_credentials = malloc(sizeof(hashmap_t));
    FILE *file = fopen("metadata.bin", "rb");
    if (file == NULL)
        return -1;
    fread(user_credentials, sizeof(hashmap_t), 1, file);
    fclose(file);
    user_credentials->entries = malloc(sizeof(static_entry) * user_credentials->capacity);
    file = fopen("user_credentials.bin", "rb");
    if (file == NULL) {
        free(user_credentials->entries);
        free(user_credentials);
        user_credentials = NULL;
        return -1;
    }
    fread(user_credentials->entries, sizeof(static_entry), user_credentials->size, file);
    fclose(file);
}

static int _persist_user_credentials()
{
    FILE *file = fopen("metadata.bin", "wb");
    if (file == NULL)
        return -1;
    fwrite(user_credentials, sizeof(hashmap_t), 1, file);
    fclose(file);
    file = fopen("user_credentials.bin", "wb");
    if (file == NULL)
        return -1;
    fwrite(user_credentials->entries, sizeof(static_entry), user_credentials->size, file);
    fclose(file);
    return 0;
}

uint8_t credentials_are_valid(const char *username, const char *password)
{
    if (user_credentials == NULL && _fetch_user_credentials() == -1){
        user_credentials = create_hashmap(HASHMAP_SIZE);
    }

    const char *stored_password = hashmap_get(user_credentials, username);

    if (stored_password == NULL){
        hashmap_insert(user_credentials, username, password);
        _persist_user_credentials();
        return 0;
    }
    else if (strcmp(stored_password, password) == 0)
        return 0;
    else
        return 1;
}