#include "../include/request_handling.h"
#include "../include/user_db.h"
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef TEST_BUILD
sqlite3 *db = NULL;
#else
extern sqlite3 *db;
#endif


// forward dec
static enum MHD_Result handle_options(struct MHD_Connection *connection, struct MHD_Response *response);
static enum MHD_Result handle_success(struct MHD_Connection *connection, struct MHD_Response *response, status_t code);
static enum MHD_Result handle_not_found(struct MHD_Connection *connection, struct MHD_Response *response, status_t code);
static enum MHD_Result handle_bad_request(struct MHD_Connection *connection, struct MHD_Response *response, status_t code);
static enum MHD_Result handle_internal_server_error(struct MHD_Connection *connection, struct MHD_Response *response, status_t code);
static const char *user_error_str(status_t code);

// mini handlers 
static enum MHD_Result handle_options(struct MHD_Connection *connection, struct MHD_Response *response) {
  enum MHD_Result ret;

  response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
  MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

static enum MHD_Result handle_success(struct MHD_Connection *connection, struct MHD_Response *response, status_t code) {
    enum MHD_Result ret;
    const char *msg = user_error_str(code);

    response = MHD_create_response_from_buffer(strlen(msg), (void *)msg,
        MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods",
        "POST, GET, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers",
        "Content-Type");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

    MHD_destroy_response(response);
    return ret;
}

static enum MHD_Result handle_bad_request(struct MHD_Connection *connection, struct MHD_Response *response, status_t code) {
  enum MHD_Result ret;
  const char *msg = user_error_str(code);

  response = MHD_create_response_from_buffer(strlen(msg), (void *)msg,
      MHD_RESPMEM_PERSISTENT);
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
  MHD_add_response_header(response, "Access-Control-Allow-Methods",
      "POST, GET, OPTIONS");
  MHD_add_response_header(response, "Access-Control-Allow-Headers",
      "Content-Type");
  ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
  MHD_destroy_response(response);
  return ret;
}

// why do i need enum before every MHD-Result?
static enum MHD_Result handle_internal_server_error(struct MHD_Connection *connection, struct MHD_Response *response, status_t code) {
  enum MHD_Result ret;
  const char *msg = user_error_str(code);

  response = MHD_create_response_from_buffer(strlen(msg), (void *)msg, MHD_RESPMEM_PERSISTENT);
  MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
  MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
  ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
  MHD_destroy_response(response);
  return ret;
}

static enum MHD_Result handle_not_found(struct MHD_Connection *connection, struct MHD_Response *response, status_t code) {
  enum MHD_Result ret;
  const char *not_found = user_error_str(code);

  response = MHD_create_response_from_buffer(
      strlen(not_found), (void *)not_found, MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
  MHD_destroy_response(response);
  return ret;
}

/*
 * Utilities
 */
// there might be some codes not used. grep pls.
static const char *user_error_str(status_t code) {
  char *str = "";
  switch (code) {
    case ERROR_INVALID_EMAIL:
      str = "incorrect email";
      break;
    case ERROR_INVALID_PASSWORD:
      str = "incorrect password";
      break;
    case ERROR_LOGIN_USER:
      str = "error logging user in";
      break;
    case ERROR_REGISTER_USER:
      str = "error registering user";
      break;
    case ERROR_UNKNOWN: // most likely sql related
      str = "server related error";
      break;
    case SUCCESS:
      str = "access granted";
      break;
    case NOT_FOUND:
      str = "page not found";
      break;
    default:
      str = "unknown error";
  }
  return str;
}

static enum MHD_Result post_iterator(void *cls, enum MHD_ValueKind kind,
    const char *key, const char *filename,
    const char *content_type,
    const char *transfer_encoding,
    const char *data, uint64_t off,
    size_t size) {
  ConnInfo *user_info = cls;

  // put data received in user_info
  if (0 == strcmp("first_name", key)) {
    user_info->first_name = strndup(data, size);
    if (!user_info->first_name) {
      fprintf(stderr, "error allocating using strndup (post_iterator)");
      return MHD_NO;
    }
  }
  if (0 == strcmp("last_name", key)) {
    user_info->last_name = strndup(data, size);
    if (!user_info->last_name) {
      fprintf(stderr, "error allocating using strndup (post_iterator)");
      return MHD_NO;
    }
  }
  if (0 == strcmp("email", key)) {
    user_info->email = strndup(data, size);
    if (!user_info->email) {
      fprintf(stderr, "error allocating using strndup (post_iterator)");
      return MHD_NO;
    }
  }
  if (0 == strcmp("password", key)) {
    user_info->password = strndup(data, size);
    if (!user_info->password) {
      fprintf(stderr, "error allocating using strndup (post_iterator)");
      return MHD_NO;
    }
  }

  return MHD_YES;
}


static enum MHD_Result
handle_request(void *cls, struct MHD_Connection *connection, const char *url,
    const char *method, const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls) {
  struct MHD_Response *response;
  // int ret;

  if (strcmp(method, "OPTIONS") == 0) {
    return handle_options(connection, response);
  }

  ConnInfo *user_info;
  if (*con_cls == NULL) {
    user_info = malloc(sizeof(ConnInfo));
    if (!user_info) {
      return MHD_NO;
    }
    memset(user_info, 0, sizeof(ConnInfo));
    *con_cls = user_info;
  } else {
    user_info = *con_cls;
  }

  if (strcmp(url, "/register") == 0 && strcmp(method, "POST") == 0) {
    if (user_info->pp == NULL) {
      user_info->pp = MHD_create_post_processor(connection, 1024,
          &post_iterator, user_info);
      return MHD_YES;
    }
    if (*upload_data_size) {
      MHD_post_process(user_info->pp, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;
    } else {
      if (!user_info->first_name || !user_info->last_name ||
          !user_info->email || !user_info->password) {
        // i think some things need to be freed here... 
        return handle_bad_request(connection, response, ERROR_REGISTER_USER);
      }

      // change status_t to status_t
      status_t inserted_user = insert_user(db, user_info);
      if (inserted_user != SUCCESS) {
        MHD_destroy_post_processor(user_info->pp);
        destroy_conn_info(user_info);
        return handle_internal_server_error(connection, response, inserted_user);
      }

      MHD_destroy_post_processor(user_info->pp);
      destroy_conn_info(user_info);

      return handle_success(connection, response, inserted_user);
    }
  } else if (strcmp(method, "POST") == 0 && strcmp(url, "/login") == 0) { // if instead of else if?
    user_error_t login_user = check_user(db, user_info);
    if (login_user != SUCCESS) {
      MHD_destroy_post_processor(user_info->pp);
      destroy_conn_info(user_info);

      return handle_internal_server_error(connection, response, login_user);
    }

    MHD_destroy_post_processor(user_info->pp);
    destroy_conn_info(user_info);

    return handle_success(connection, response, login_user);
  } else {
    destroy_conn_info(user_info);
    return handle_not_found(connection, response, NOT_FOUND);
  }
  return MHD_NO;
}

int MHD_background(int argc, char *const *argv) {
  struct MHD_Daemon *d;
  struct timeval tv;
  struct timeval *tvp;
  fd_set rs;
  fd_set ws;
  fd_set es;
  MHD_socket max;
  MHD_UNSIGNED_LONG_LONG mhd_timeout;

  if (argc != 2) {
    printf("%s PORT\n", argv[0]);
    return 1;
  }
  /* initialize PRNG */
  srandom((unsigned int)time(NULL));
  d = MHD_start_daemon(MHD_USE_DEBUG, atoi(argv[1]), NULL, NULL, &handle_request,
      NULL, MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)15,
      MHD_OPTION_NOTIFY_COMPLETED, NULL, NULL, MHD_OPTION_END);
  if (NULL == d)
    return 1;
  while (1) {
    max = 0;
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);
    if (MHD_YES != MHD_get_fdset(d, &rs, &ws, &es, &max))
      break; /* fatal internal error */
    if (MHD_get_timeout(d, &mhd_timeout) == MHD_YES) {
      tv.tv_sec = mhd_timeout / 1000;
      tv.tv_usec = (mhd_timeout - (tv.tv_sec * 1000)) * 1000;
      tvp = &tv;
    } else
      tvp = NULL;
    select(max + 1, &rs, &ws, &es, tvp);
    MHD_run(d);
  }
  MHD_stop_daemon(d);
  return 0;
}
