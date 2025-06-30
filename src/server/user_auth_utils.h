#ifndef USER_AUTH_UTILS_H
#define USER_AUTH_UTILS_H

/*returns 0x00 if the password is correct or if the user didn't exist, 
in which case it also adds the pair username/password to the database. 
If it is incorrect it returns 0x01*/
uint8_t credentials_are_valid(const char *username, const char *password);

#endif