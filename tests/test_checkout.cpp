#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include <unistd.h>

#include "checkout.h"
#include "commit.h"
#include "object_store.h"
#include "tree.h"

// 本文件包含针对 checkout 工作区重建逻辑的单元测试

// 帮助函数：在目录下创建一个简单文件
static bool write_text_file(const std::string& path, const std::string& text) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        return false;
    }
    if (!text.empty()) {
        std::size_t written =
            std::fwrite(text.data(), 1, text.size(), fp);
        if (written != text.size()) {
            std::fclose(fp);
            return false;
        }
    }
    std::fclose(fp);
    return true;
}

// 验证根据 tree 哈希进行 checkout，能够在空目录中重建文件
TEST(CheckoutTest, CheckoutTreeIntoEmptyDir) {
    char work_tmpl[] = "/tmp/minigit_checkout_workXXXXXX";
    char* work_dir_c = mkdtemp(work_tmpl);
    ASSERT_NE(work_dir_c, nullptr);
    std::string work_dir(work_dir_c);

    char repo_tmpl[] = "/tmp/minigit_checkout_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    std::string file_path = work_dir + "/hello.txt";
    ASSERT_TRUE(write_text_file(file_path, "hello checkout"));

    minigit::ObjectStore store(repo_dir);
    std::string tree_hash = minigit::write_tree(store, work_dir);
    ASSERT_FALSE(tree_hash.empty());

    std::string other_path = work_dir + "/to_be_removed.txt";
    ASSERT_TRUE(write_text_file(other_path, "temp"));

    bool ok = minigit::checkout_tree(store, work_dir, tree_hash);
    ASSERT_TRUE(ok);

    std::string data;
    {
        std::FILE* fp = std::fopen(file_path.c_str(), "rb");
        ASSERT_NE(fp, nullptr);
        char buffer[64];
        std::size_t n = std::fread(buffer, 1, sizeof(buffer), fp);
        std::fclose(fp);
        data.assign(buffer, buffer + n);
    }
    EXPECT_EQ(data, "hello checkout");

    std::FILE* fp2 = std::fopen(other_path.c_str(), "rb");
    if (fp2) {
        std::fclose(fp2);
    }
    EXPECT_EQ(fp2, nullptr);
}

// 验证根据 commit 哈希进行 checkout，能够恢复 tree 对应的文件
TEST(CheckoutTest, CheckoutCommitRestoresTree) {
    char work_tmpl[] = "/tmp/minigit_checkout_workXXXXXX";
    char* work_dir_c = mkdtemp(work_tmpl);
    ASSERT_NE(work_dir_c, nullptr);
    std::string work_dir(work_dir_c);

    char repo_tmpl[] = "/tmp/minigit_checkout_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    std::string file_path = work_dir + "/foo.txt";
    ASSERT_TRUE(write_text_file(file_path, "foo"));

    minigit::ObjectStore store(repo_dir);
    std::string tree_hash = minigit::write_tree(store, work_dir);
    ASSERT_FALSE(tree_hash.empty());

    minigit::Commit c;
    c.tree = tree_hash;
    c.author = "A <a@example.com> 0 +0000";
    c.committer = "C <c@example.com> 0 +0000";
    c.message = "msg";
    std::string commit_hash = minigit::write_commit(store, c);
    ASSERT_FALSE(commit_hash.empty());

    ASSERT_TRUE(write_text_file(file_path, "bar"));

    bool ok = minigit::checkout_commit(store, work_dir, commit_hash);
    ASSERT_TRUE(ok);

    std::string data;
    {
        std::FILE* fp = std::fopen(file_path.c_str(), "rb");
        ASSERT_NE(fp, nullptr);
        char buffer[64];
        std::size_t n = std::fread(buffer, 1, sizeof(buffer), fp);
        std::fclose(fp);
        data.assign(buffer, buffer + n);
    }
    EXPECT_EQ(data, "foo");
}

