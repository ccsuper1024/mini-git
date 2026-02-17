#include <gtest/gtest.h>

#include <string>

#include <unistd.h>

#include "filesystem.h"
#include "refs.h"

// 本文件包含针对 refs 与 HEAD 管理逻辑的单元测试

// 验证 HEAD 符号引用的写入与读取
TEST(RefsTest, HeadSymbolicRoundtrip) {
    char repo_tmpl[] = "/tmp/minigit_refs_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::FileSystem fs(repo_dir);

    std::string refname = "refs/heads/master";
    bool ok = minigit::set_head_symbolic(fs, refname);
    ASSERT_TRUE(ok);

    minigit::Head head;
    ok = minigit::read_head(fs, head);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(head.symbolic);
    EXPECT_EQ(head.target, refname);
}

// 验证 HEAD 游离状态的写入与读取
TEST(RefsTest, HeadDetachedRoundtrip) {
    char repo_tmpl[] = "/tmp/minigit_refs_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::FileSystem fs(repo_dir);

    std::string hash = "1111111111111111111111111111111111111111";
    bool ok = minigit::set_head_detached(fs, hash);
    ASSERT_TRUE(ok);

    minigit::Head head;
    ok = minigit::read_head(fs, head);
    ASSERT_TRUE(ok);
    EXPECT_FALSE(head.symbolic);
    EXPECT_EQ(head.target, hash);
}

// 验证 ref 文件的更新与读取，包括自动创建目录
TEST(RefsTest, UpdateAndReadRef) {
    char repo_tmpl[] = "/tmp/minigit_refs_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::FileSystem fs(repo_dir);

    std::string refname = "refs/heads/dev";
    std::string hash = "2222222222222222222222222222222222222222";

    bool ok = minigit::update_ref(fs, refname, hash);
    ASSERT_TRUE(ok);

    std::string read_hash;
    ok = minigit::read_ref(fs, refname, read_hash);
    ASSERT_TRUE(ok);
    EXPECT_EQ(read_hash, hash);
}

// 验证 HEAD 指向某个分支，并通过 ref 获取对应哈希的组合流程
TEST(RefsTest, HeadPointsToBranchRef) {
    char repo_tmpl[] = "/tmp/minigit_refs_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::FileSystem fs(repo_dir);

    std::string branch_ref = "refs/heads/feature";
    std::string hash = "3333333333333333333333333333333333333333";

    bool ok = minigit::update_ref(fs, branch_ref, hash);
    ASSERT_TRUE(ok);

    ok = minigit::set_head_symbolic(fs, branch_ref);
    ASSERT_TRUE(ok);

    minigit::Head head;
    ok = minigit::read_head(fs, head);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(head.symbolic);
    EXPECT_EQ(head.target, branch_ref);

    std::string resolved_hash;
    ok = minigit::read_ref(fs, head.target, resolved_hash);
    ASSERT_TRUE(ok);
    EXPECT_EQ(resolved_hash, hash);
}

