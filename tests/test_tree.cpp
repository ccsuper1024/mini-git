#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include "object_store.h"
#include "tree.h"

// 本文件包含针对 tree 对象构建与解析的单元测试

// 验证 build_tree_object 与 parse_tree_object 的互逆性
TEST(TreeTest, BuildAndParseRoundtrip) {
    minigit::TreeEntry e1;
    e1.mode = "100644";
    e1.name = "file.txt";
    e1.hash = "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed";

    minigit::TreeEntry e2;
    e2.mode = "40000";
    e2.name = "dir";
    e2.hash = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

    std::vector<minigit::TreeEntry> entries;
    entries.push_back(e1);
    entries.push_back(e2);

    std::string content = minigit::build_tree_object(entries);

    std::vector<minigit::TreeEntry> parsed;
    bool ok = minigit::parse_tree_object(content, parsed);
    EXPECT_TRUE(ok);
    ASSERT_EQ(parsed.size(), entries.size());

    EXPECT_EQ(parsed[0].mode, entries[0].mode);
    EXPECT_EQ(parsed[0].name, entries[0].name);
    EXPECT_EQ(parsed[0].hash, entries[0].hash);

    EXPECT_EQ(parsed[1].mode, entries[1].mode);
    EXPECT_EQ(parsed[1].name, entries[1].name);
    EXPECT_EQ(parsed[1].hash, entries[1].hash);
}

// 在临时目录中构建目录快照，并验证 tree 与 blob 的读写
TEST(TreeTest, WriteTreeFromFileSystem) {
    // 创建工作目录
    char work_tmpl[] = "/tmp/minigit_workXXXXXX";
    char* work_dir_c = mkdtemp(work_tmpl);
    ASSERT_NE(work_dir_c, nullptr);
    std::string work_dir(work_dir_c);

    // 在工作目录中创建一个文件
    std::string file_path = work_dir + "/hello.txt";
    {
        std::FILE* fp = std::fopen(file_path.c_str(), "wb");
        ASSERT_NE(fp, nullptr);
        const char* msg = "hello tree";
        std::fwrite(msg, 1, std::strlen(msg), fp);
        std::fclose(fp);
    }

    // 创建仓库根目录
    char repo_tmpl[] = "/tmp/minigit_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::ObjectStore store(repo_dir);

    // 写入目录快照
    std::string tree_hash = minigit::write_tree(store, work_dir);
    ASSERT_FALSE(tree_hash.empty());

    // 读取顶层 tree 对象并解析
    std::string tree_content;
    bool ok = store.read_object(tree_hash, tree_content);
    ASSERT_TRUE(ok);

    std::vector<minigit::TreeEntry> entries;
    ok = minigit::parse_tree_object(tree_content, entries);
    ASSERT_TRUE(ok);
    ASSERT_EQ(entries.size(), 1U);

    const minigit::TreeEntry& e = entries[0];
    EXPECT_EQ(e.mode, "100644");
    EXPECT_EQ(e.name, "hello.txt");

    // 验证对应 blob 对象内容
    std::string blob_data;
    ok = store.read_object(e.hash, blob_data);
    ASSERT_TRUE(ok);
    EXPECT_EQ(blob_data, "hello tree");
}
