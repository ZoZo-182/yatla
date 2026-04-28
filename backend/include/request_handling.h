#ifndef REQUEST_HANDLING_H
#define REQUEST_HANDLING_H

#include <stdbool.h>
#include <microhttpd.h>


// struct for user register data + pp struct for post req
// rename ConnInfo to []_t
typedef struct 
{
    struct MHD_PostProcessor *pp;
    char *first_name;
    char *last_name;
    char *email;
    char *password;
} ConnInfo;

typedef enum
{
    ERROR_INVALID_EMAIL,
    ERROR_INVALID_PASSWORD,
    ERROR_UNKNOWN,
    SUCCESS,
    ERROR_REGISTER_USER,
    ERROR_LOGIN_USER,
    NOT_FOUND,
} status_t;

int MHD_background(int argc, char *const *argv); 

#endif
