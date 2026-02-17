#include <gtest/gtest.h>

#include "hash.h"

// 本文件包含针对 SHA-1 哈希实现的单元测试

// 验证空字符串的哈希值是否符合标准
TEST(HashTest, EmptyString) {
    EXPECT_EQ(minigit::sha1_hex(""),
              "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

// 验证常见字符串 "hello world" 的哈希值是否正确
TEST(HashTest, HelloWorld) {
    EXPECT_EQ(minigit::sha1_hex("hello world"),
              "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed");
}
