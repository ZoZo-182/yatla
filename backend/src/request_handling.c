#include "../include/request_handling.h"
#include "../include/user_db.h"
#include <microhttpd.h>
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifdef TEST_BUILD
sqlite3 *db = NULL;
#else
extern sqlite3 *db;
#endif

#define UNUSED(x) ((void)(x))


// forward dec
static enum MHD_Result handle_options(struct MHD_Connection *connection);
static enum MHD_Result handle_success(struct MHD_Connection *connection, status_t code);
static enum MHD_Result handle_not_found(struct MHD_Connection *connection, status_t code);
static enum MHD_Result handle_bad_request(struct MHD_Connection *connection, status_t code);
static enum MHD_Result handle_internal_server_error(struct MHD_Connection *connection, status_t code);
static const char *user_error_str(status_t code);
static enum MHD_Result add_cors_headers(struct MHD_Response *response);
static enum MHD_Result send_text_response(struct MHD_Connection *connection, unsigned int status_code, const char *body);
static void request_callback(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode termination_code);
static enum MHD_Result write_form_field(char **field, const char *data, uint64_t offset, size_t size);


static enum MHD_Result add_cors_headers(struct MHD_Response *response) {
  if (MHD_add_response_header(response, "Access-Control-Allow-Origin", "*") != MHD_YES) return MHD_NO;
  if (MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS") != MHD_YES) return MHD_NO;
  if (MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization") != MHD_YES) return MHD_NO;

  return MHD_YES;
}

static enum MHD_Result send_text_response(struct MHD_Connection *connection, unsigned int status_code, const char *body) {
  struct MHD_Response *response = MHD_create_response_from_buffer(strlen(body), (void *)body, MHD_RESPMEM_PERSISTENT);
  if (!response) {
    return MHD_NO;
  }

  if (add_cors_headers(response) != MHD_YES || MHD_add_response_header(response, "Content-Type", "text/plain; charset=utf-8") != MHD_YES) {
    MHD_destroy_response(response);
    return MHD_NO;
  }

  enum MHD_Result ret = MHD_queue_response(connection, status_code, response);
  MHD_destroy_response(response);

  return ret;
}

static void request_callback(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode termination_code) {
  UNUSED(cls);
  UNUSED(connection);
  UNUSED(termination_code);

  ConnInfo *user_info = *con_cls;

  if (!user_info) {
    return;
  }

  if (user_info->pp) {
    MHD_destroy_post_processor(user_info->pp);
  }

  destroy_conn_info(user_info);
  *con_cls = NULL;
}

static enum MHD_Result write_form_field(char **field, const char *data, uint64_t offset, size_t size) {
  if (offset >= SIZE_MAX || size > SIZE_MAX - (size_t)offset - 1) {
     return MHD_NO;
  }

  size_t required_size = size + (size_t)offset + 1;

  // realloc space for the whole thing
  char *resized = realloc(*field, required_size);
  if (!resized) {
    return MHD_NO;
  }

  // move pointer so new chunck isnt overwritting prev chunck
  memcpy(resized + offset, data, size);
  resized[size + offset] = '\0';
  *field = resized;

  return MHD_YES;
}

/***********
 * HANDLERS
 ***********/
static enum MHD_Result handle_options(struct MHD_Connection *connection) {
  return send_text_response(connection, MHD_HTTP_OK, "");
}

static enum MHD_Result handle_success(struct MHD_Connection *connection, status_t code) {
  return send_text_response(connection, MHD_HTTP_OK, user_error_str(code));
}

static enum MHD_Result handle_bad_request(struct MHD_Connection *connection, status_t code) {
  return send_text_response(connection, MHD_HTTP_BAD_REQUEST, user_error_str(code));
}

static enum MHD_Result handle_internal_server_error(struct MHD_Connection *connection, status_t code) {
  return send_text_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, user_error_str(code));
}

static enum MHD_Result handle_not_found(struct MHD_Connection *connection, status_t code) {
  return send_text_response(connection, MHD_HTTP_NOT_FOUND, user_error_str(code));
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

static enum MHD_Result post_iterator(void *cls, enum MHD_ValueKind kind, const char *key, const char *filename,
    const char *content_type, const char *transfer_encoding, const char *data, uint64_t off, size_t size) {
  UNUSED(kind);
  UNUSED(content_type);
  UNUSED(transfer_encoding);
  UNUSED(filename);

  ConnInfo *user_info = cls;

  // put data received in user_info
  if (0 == strcmp("first_name", key)) {
    return write_form_field(&user_info->first_name, data, off, size);
  }

  if (0 == strcmp("last_name", key)) {
    return write_form_field(&user_info->last_name, data, off, size);
  }

  if (0 == strcmp("email", key)) {
    return write_form_field(&user_info->email, data, off, size);
  }

  if (0 == strcmp("password", key)) {
    return write_form_field(&user_info->password, data, off, size);
  }

  return MHD_YES;
}

static enum MHD_Result handle_request(void *cls, struct MHD_Connection *connection, const char *url,
    const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {

  if (strcmp(method, "OPTIONS") == 0) {
    return handle_options(connection);
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
      user_info->pp = MHD_create_post_processor(connection, 1024, &post_iterator, user_info);
      return MHD_YES;
    }
    if (*upload_data_size) {
      MHD_post_process(user_info->pp, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;
    } 
      if (!user_info->first_name || !user_info->last_name || !user_info->email || !user_info->password) {
        return handle_bad_request(connection, ERROR_REGISTER_USER);
      }

      status_t inserted_user = insert_user(db, user_info);
      if (inserted_user != SUCCESS) {
        return handle_internal_server_error(connection, inserted_user);
      }

      return handle_success(connection, inserted_user);
    
  } else if (strcmp(method, "POST") == 0 && strcmp(url, "/login") == 0) { // if instead of else if?
    if (user_info->pp == NULL) {
      user_info->pp = MHD_create_post_processor(connection, 1024, &post_iterator, user_info);
      return MHD_YES;
    }

    if (*upload_data_size) {
      MHD_post_process(user_info->pp, upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;
    } 

    if (!user_info->email || !user_info->password) {
      return handle_bad_request(connection, ERROR_LOGIN_USER);
    }

    status_t login_user = check_user(db, user_info); 

    if (login_user != SUCCESS) {
      return handle_bad_request(connection, login_user);
    }

    return handle_success(connection, login_user);

  } else {
    return handle_not_found(connection, NOT_FOUND);
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
      MHD_OPTION_NOTIFY_COMPLETED, &request_callback, NULL, MHD_OPTION_END);
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
