#ifndef USER_AUTH_UTILS_H
#define USER_AUTH_UTILS_H

/*returns 0 if user doesn't exist, 1 if it does*/
int user_exists(const char *username);

/*returns 0 if password is incorrect, 1 if it is correct*/
int password_is_correct(const char *username, const char *password);

/* Adds a user with the given username and password.
   Returns 0 on success, -1 on failure (if the user already exists).
*/
void add_user(const char *username, const char *password);

#endif