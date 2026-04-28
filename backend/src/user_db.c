#include <stdio.h>
#include <sodium.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "../include/user_db.h"


char *hash_password(char *password) {
  char *hashed_password = malloc(crypto_pwhash_STRBYTES);
  if (!hashed_password) {
    return NULL;
  }

  if (crypto_pwhash_str(hashed_password, password, strlen(password), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
    fprintf(stderr, "error hashing password");
    free(hashed_password);
    return NULL;
  }

  return hashed_password; // remember to free this after function call
}

status_t insert_user(sqlite3 *db, ConnInfo *user_info) {
  sqlite3_stmt *statement;
  char *hashed_password = hash_password(user_info->password);

  // parameter binding instead of using sprintf (unsafe!)
  const char *sql = "INSERT INTO users (first_name, last_name, email, password) VALUES (?, ?, ?, ?)";

  int rc = sqlite3_prepare_v2(db, sql, strlen(sql), &statement, NULL);
  if (rc == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, user_info->first_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, user_info->last_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, user_info->email, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, hashed_password, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE) {
      fprintf(stderr, "error executing sql statement: %s (insert_user)\n", sqlite3_errmsg(db));
      sqlite3_finalize(statement);
      return ERROR_UNKNOWN;
    }
    sqlite3_finalize(statement);
    free(hashed_password);
    return SUCCESS;
  } else {
    free(hashed_password);
    return ERROR_UNKNOWN;
  }
}

status_t check_user(sqlite3 *db, ConnInfo *user_info) {
  sqlite3_stmt *statement;

  // parameter binding instead of using sprintf (unsafe!)
  const char *sql = "SELECT email, password FROM users WHERE email= ?";

  int rc = sqlite3_prepare_v2(db, sql, strlen(sql), &statement, NULL);
  if (rc == SQLITE_OK) {
    // compare client user info to user creds in database
    sqlite3_bind_text(statement, 1, user_info->email, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
      fprintf(stderr, "error executing sql statement: %s (check_user)\n", sqlite3_errmsg(db));
      sqlite3_finalize(statement);
      return ERROR_INVALID_EMAIL;
    }

    int correct_password = crypto_pwhash_str_verify(sqlite3_column_text(statement, 1), user_info->password, strlen(user_info->password));
    /* more than once you have looked at this statement, forgot what it
     * does, and freaked out about it not being !correct_password.
     * look at the damn return type of correct_password -1 vs 0*/
    if (correct_password) {
      sqlite3_finalize(statement);
      return ERROR_INVALID_PASSWORD;
    }

    sqlite3_finalize(statement);
    return SUCCESS;
  } else {
    return ERROR_UNKNOWN;
  }
}

void destroy_conn_info(ConnInfo *user_info) {
  free(user_info->first_name);
  free(user_info->last_name);
  free(user_info->email);
  free(user_info->password);
  free(user_info);
}
