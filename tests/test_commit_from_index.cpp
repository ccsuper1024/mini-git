#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include <unistd.h>

#include "commit.h"
#include "filesystem.h"
#include "index.h"
#include "object_store.h"
#include "refs.h"
#include "tree.h"

// 本文件验证通过 index 构建 tree 并生成 commit 的流程

static bool write_text_file(const std::string& path, const std::string& text) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    if (!text.empty()) {
        std::size_t written = std::fwrite(text.data(), 1, text.size(), fp);
        if (written != text.size()) {
            std::fclose(fp);
            return false;
        }
    }
    std::fclose(fp);
    return true;
}

TEST(CommitFromIndexTest, BuildTreeAndCommitThenUpdateBranch) {
    // 工作区
    char work_tmpl[] = "/tmp/minigit_commit_from_index_workXXXXXX";
    char* work_dir_c = mkdtemp(work_tmpl);
    ASSERT_NE(work_dir_c, nullptr);
    std::string work_dir(work_dir_c);

    // 仓库
    char repo_tmpl[] = "/tmp/minigit_commit_from_index_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    // 在工作区准备两个文件
    ASSERT_TRUE(write_text_file(work_dir + "/a.txt", "A"));
    ::mkdir((work_dir + "/sub").c_str(), 0777);
    ASSERT_TRUE(write_text_file(work_dir + "/sub/b.txt", "B"));

    minigit::ObjectStore store(repo_dir);
    minigit::FileSystem fs(repo_dir);

    // 模拟 add：把两个文件作为 blob 写入，并记录到 index
    std::vector<minigit::IndexEntry> entries;
    {
        // a.txt
        std::FILE* fp = std::fopen((work_dir + "/a.txt").c_str(), "rb");
        ASSERT_NE(fp, nullptr);
        char buf[64];
        std::size_t n = std::fread(buf, 1, sizeof(buf), fp);
        std::fclose(fp);
        std::string data(buf, buf + n);
        std::string h = store.store_blob(data);
        entries.push_back(minigit::IndexEntry{"100644", "a.txt", h});
    }
    {
        // sub/b.txt
        std::FILE* fp = std::fopen((work_dir + "/sub/b.txt").c_str(), "rb");
        ASSERT_NE(fp, nullptr);
        char buf[64];
        std::size_t n = std::fread(buf, 1, sizeof(buf), fp);
        std::fclose(fp);
        std::string data(buf, buf + n);
        std::string h = store.store_blob(data);
        entries.push_back(minigit::IndexEntry{"100644", "sub/b.txt", h});
    }
    ASSERT_TRUE(minigit::write_index(fs, entries));

    // 设置 HEAD 指向 master 分支
    ASSERT_TRUE(minigit::set_head_symbolic(fs, "refs/heads/master"));

    // 根据 index 构建 tree
    std::vector<minigit::IndexEntry> loaded;
    ASSERT_TRUE(minigit::read_index(fs, loaded));
    std::string tree_hash = minigit::write_tree_from_index(store, loaded);
    ASSERT_FALSE(tree_hash.empty());

    // 生成 commit，并更新分支
    minigit::Head head;
    ASSERT_TRUE(minigit::read_head(fs, head));
    ASSERT_TRUE(head.symbolic);

    std::string parent_hash;  // 首次提交没有父
    minigit::Commit c;
    c.tree = tree_hash;
    c.author = "U <u@example.com> 0 +0000";
    c.committer = "U <u@example.com> 0 +0000";
    c.message = "init";
    std::string commit_hash = minigit::write_commit(store, c);
    ASSERT_FALSE(commit_hash.empty());

    ASSERT_TRUE(minigit::update_ref(fs, head.target, commit_hash));

    // 读取分支当前 commit 并验证解析
    std::string got_hash;
    ASSERT_TRUE(minigit::read_ref(fs, head.target, got_hash));
    EXPECT_EQ(got_hash, commit_hash);

    std::string commit_body;
    ASSERT_TRUE(store.read_object(commit_hash, commit_body));
    minigit::Commit parsed;
    ASSERT_TRUE(minigit::parse_commit_object(commit_body, parsed));
    EXPECT_EQ(parsed.tree, tree_hash);

    // 读取 tree 并验证条目包含 a.txt 与子目录 sub
    std::string tree_body;
    ASSERT_TRUE(store.read_object(tree_hash, tree_body));
    std::vector<minigit::TreeEntry> root_entries;
    ASSERT_TRUE(minigit::parse_tree_object(tree_body, root_entries));

    bool has_a = false;
    bool has_sub = false;
    std::string sub_hash;
    for (const auto& e : root_entries) {
        if (e.mode == "100644" && e.name == "a.txt") {
            has_a = true;
        } else if (e.mode == "40000" && e.name == "sub") {
            has_sub = true;
            sub_hash = e.hash;
        }
    }
    EXPECT_TRUE(has_a);
    EXPECT_TRUE(has_sub);
    ASSERT_FALSE(sub_hash.empty());

    // 验证子目录 tree 包含 b.txt
    std::string sub_body;
    ASSERT_TRUE(store.read_object(sub_hash, sub_body));
    std::vector<minigit::TreeEntry> sub_entries;
    ASSERT_TRUE(minigit::parse_tree_object(sub_body, sub_entries));
    bool has_b = false;
    for (const auto& e : sub_entries) {
        if (e.mode == "100644" && e.name == "b.txt") {
            has_b = true;
        }
    }
    EXPECT_TRUE(has_b);
}
