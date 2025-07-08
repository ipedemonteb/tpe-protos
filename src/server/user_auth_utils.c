#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "../utils/hashmap.h"
#include "include/user_auth_utils.h"

#define HASHMAP_SIZE 32

hashmap_t *user_credentials = NULL;

void free_user_credentials()
{
    if (user_credentials)
    {
        destroy_hashmap(user_credentials);
        user_credentials = NULL;
    }
}

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
    return 0;
}

static int _persist_user_credentials()
{
    if (user_credentials == NULL || user_credentials->entries == NULL)
        return -1;

    FILE *file = fopen("metadata.bin", "wb");
    if (file == NULL)
        return -1;
    if (fwrite(user_credentials, sizeof(hashmap_t), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);

    file = fopen("user_credentials.bin", "wb");
    if (file == NULL)
        return -1;
    if (fwrite(user_credentials->entries, sizeof(static_entry), user_credentials->size, file) != user_credentials->size) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

uint8_t credentials_are_valid(const char *username, const char *password)
{
    if (user_credentials == NULL && _fetch_user_credentials() == -1){
        user_credentials = create_hashmap(HASHMAP_SIZE);
        printf("\n\n\nNo user credentials found, creating new hashmap\n\n\n");
    }

    const static_entry *user_password = hashmap_get(user_credentials, username);
    if (user_password == NULL){
        printf("\n\n\nAdding new user: %s\n\n\n", username);
        hashmap_insert(user_credentials, username, password);
        printf("\nHICE EL INSERT\n");

        for (size_t i = 0; i < user_credentials->capacity; i++) {
            printf("Key: %s, Value: %s\n", user_credentials->entries[i].key, user_credentials->entries[i].value);
        }
        user_password = hashmap_get(user_credentials, username);
        printf("abc");
        printf("\n\n\n Stored pass: %s for user: %s\n\n\n", user_password->value, user_password->key);
        _persist_user_credentials();
        printf("\n\n\nUser %s added successfully\n\n\n", username);
        return 0x00;
    }
    else if (strcmp(user_password->value, password) == 0)
        return 0x00;
    else
        return 0x01;
}