#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include <unistd.h>

#include "object_store.h"

// 本文件包含针对对象存储 ObjectStore 的集成测试

// 在临时目录中存储并读取一个 blob，验证读写完整性
TEST(ObjectStoreTest, StoreAndReadBlob) {
    char tmpl[] = "/tmp/minigit_testXXXXXX";
    char* dir = mkdtemp(tmpl);
    ASSERT_NE(dir, nullptr);

    std::string root(dir);
    minigit::ObjectStore store(root);

    std::string data = "hello object store";
    std::string hash = store.store_blob(data);

    std::string out;
    bool ok = store.read_object(hash, out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, data);
}
