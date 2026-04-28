#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "../include/oauth.h"

#define STR_SIZE 10000


CURL *curl; 

void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

char *build_github_auth_url(void) {
  char *url = "https://github.com/login/oauth/authorize?";
  const char *gh_client_id = getenv("GITHUB_CLIENT_ID");
  const char *redirect_uri = getenv("callback");

  char state[65];
  rand_str(state, sizeof state);

  snprintf(url, STR_SIZE,
      "client_id=%s&redirect_uri=%s&scope=read:user&state=%s",
      gh_client_id, redirect_uri, state);

  return url;
}

char *build_google_auth_url(void) {
  char *url = "https://accouts.google.com/o/oauth2/v2/auth?";
  const char *gg_client_id = getenv("GOOGLE_CLIENT_ID");
  const char *redirect_uri = getenv("callback"); // confirm this is what you set google's as

  char state[65];
  rand_str(state, sizeof state);

  snprintf(url, STR_SIZE,
      "client_id=%s&redirect_uri=%s&response_type=code&scope=profile&state=%s",
      gg_client_id, redirect_uri, state);

  return url;
}

/* provider redirects back to my app with a code to the frontend which sends
 * it to the backend to exchange the code for access token. does this by
 * sending post req to provider with the access token and the body of the 
 * token contains the client id, secret, code and redirect url
 */
char *exchange_code_for_access_token(const char *code, const char *provider ) {
  // I think the code will contain the info I need to make an access token...
  // send a post request using provider with libcurl functions. The access token is the post/sending data, the provider is dest.
  // I am assuming the access token contains a header + body.
    // figure out what the head should consist of (id?) 
}

/* backend does a get req to provider to get the user's info. JWT is
 * created and it is returned to frontend
 */
char *get_user_info_from_provider(const char *access_token, const char *provider) {
  // at this point my app's server now has access to the user's info, so do a get req w/ the access token 
    // just take first name
    // generate / store (make a new table) jwt (header + body/payload / signature(?))
    // send the jwt to the frontend
}
