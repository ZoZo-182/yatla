#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <sodium.h>

#include "../include/user_db.h"

TEST(UserPwTest, HashCanVerifyOriginalPw) {
  char password[] = "yummy";
  char wrong_password[] = "blahblahblah";

  char *hash = hash_password(password);

  ASSERT_NE(hash, nullptr);
  EXPECT_EQ(crypto_pwhash_str_verify(hash, password, std::strlen(password)), 0);

  std::free(hash);
}
