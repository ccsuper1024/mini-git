#include <gtest/gtest.h>

#include "zlib_utils.h"

// 本文件包含针对 zlib 压缩/解压工具函数的单元测试

// 验证同一段数据压缩再解压后内容保持一致
TEST(ZlibUtilsTest, Roundtrip) {
    std::string original = "mini-git zlib roundtrip test data";
    std::string compressed = minigit::zlib_compress(original);
    std::string decompressed = minigit::zlib_decompress(compressed);
    EXPECT_EQ(original, decompressed);
}
