#ifndef USER_AUTH_UTILS_H
#define USER_AUTH_UTILS_H

/*returns 0 if password is incorrect and 1 if it is correct or if the user didn't exist, 
in which case it also adds the pair username/password to the database*/
int credentials_are_valid(const char *username, const char *password);

#endif