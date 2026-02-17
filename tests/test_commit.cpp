#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

#include "commit.h"
#include "object_store.h"

// 本文件包含针对 commit 对象编码、解析与存储的单元测试

// 验证 build_commit_object 与 parse_commit_object 的互逆性
TEST(CommitTest, BuildAndParseRoundtrip) {
    minigit::Commit c;
    c.tree = "1111111111111111111111111111111111111111";
    c.parents.push_back("2222222222222222222222222222222222222222");
    c.parents.push_back("3333333333333333333333333333333333333333");
    c.author = "Alice <alice@example.com> 123456789 +0000";
    c.committer = "Bob <bob@example.com> 123456790 +0000";
    c.message = "first line\nsecond line";

    std::string content = minigit::build_commit_object(c);

    minigit::Commit parsed;
    bool ok = minigit::parse_commit_object(content, parsed);
    EXPECT_TRUE(ok);
    EXPECT_EQ(parsed.tree, c.tree);
    ASSERT_EQ(parsed.parents.size(), c.parents.size());
    EXPECT_EQ(parsed.parents[0], c.parents[0]);
    EXPECT_EQ(parsed.parents[1], c.parents[1]);
    EXPECT_EQ(parsed.author, c.author);
    EXPECT_EQ(parsed.committer, c.committer);
    EXPECT_EQ(parsed.message, c.message);
}

// 在临时仓库中写入 commit 对象并通过 ObjectStore 读回验证
TEST(CommitTest, StoreAndReadCommit) {
    char repo_tmpl[] = "/tmp/minigit_commit_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::ObjectStore store(repo_dir);

    minigit::Commit c;
    c.tree = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    c.parents.push_back("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    c.author = "Author <author@example.com> 1111111111 +0000";
    c.committer = "Committer <committer@example.com> 1111111112 +0000";
    c.message = "commit message";

    std::string hash = minigit::write_commit(store, c);
    ASSERT_EQ(hash.size(), 40U);

    std::string body;
    bool ok = store.read_object(hash, body);
    ASSERT_TRUE(ok);

    minigit::Commit parsed;
    ok = minigit::parse_commit_object(body, parsed);
    ASSERT_TRUE(ok);
    EXPECT_EQ(parsed.tree, c.tree);
    ASSERT_EQ(parsed.parents.size(), c.parents.size());
    EXPECT_EQ(parsed.parents[0], c.parents[0]);
    EXPECT_EQ(parsed.author, c.author);
    EXPECT_EQ(parsed.committer, c.committer);
    EXPECT_EQ(parsed.message, c.message);
}

