#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char username[255];
    char password[255];
} VerySecureData;




 typedef struct persistableData {
    VerySecureData *data;
    int count;
} persistableData;


  void persist_data(const char *filename, VerySecureData *data, int count) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }
    fwrite(data, sizeof(VerySecureData), count, file);
    fclose(file);

  }

  void retrieve_data(const char *filename, VerySecureData *data, int count) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Failed to open file for reading");
        return;
    }
    fread(data, sizeof(VerySecureData), count, file);
    fclose(file);
  }

int main(){


    VerySecureData *users = malloc(33 * sizeof(VerySecureData));

    /* for (int i = 0; i < 33; i++) {
        users[i].username[0] = 'u';
        users[i].username[1] = 's';
        users[i].username[2] = 'e';
        users[i].username[3] = 'r'; 
        users[i].username[4] = '0' + i;
        users[i].username[5] = '0' + i%10;
        users[i].password[0] = 'p';
        users[i].password[1] = 'a';
        users[i].password[2] = 's';         
    } 

    persist_data("users.bin", users, 33); */


    retrieve_data("users.bin", users, 33);

    for (int i = 0; i < 33; i++) {
        printf("Username: %s, Password: %s\n", users[i].username, users[i].password);
    }

    puts("FiEdi");
}