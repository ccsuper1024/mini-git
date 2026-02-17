#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <unistd.h>

#include "filesystem.h"
#include "index.h"

// 本文件包含针对 index（暂存区）读写逻辑的单元测试

// 验证 index 文件的写入与读取可以保持条目内容一致
TEST(IndexTest, WriteAndReadRoundtrip) {
    char repo_tmpl[] = "/tmp/minigit_index_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::FileSystem fs(repo_dir);

    std::vector<minigit::IndexEntry> entries;
    minigit::IndexEntry e1;
    e1.mode = "100644";
    e1.path = "foo.txt";
    e1.hash = "1111111111111111111111111111111111111111";
    entries.push_back(e1);

    minigit::IndexEntry e2;
    e2.mode = "100644";
    e2.path = "bar/baz.txt";
    e2.hash = "2222222222222222222222222222222222222222";
    entries.push_back(e2);

    bool ok = minigit::write_index(fs, entries);
    ASSERT_TRUE(ok);

    std::vector<minigit::IndexEntry> loaded;
    ok = minigit::read_index(fs, loaded);
    ASSERT_TRUE(ok);
    ASSERT_EQ(loaded.size(), entries.size());
    EXPECT_EQ(loaded[0].mode, e1.mode);
    EXPECT_EQ(loaded[0].path, e1.path);
    EXPECT_EQ(loaded[0].hash, e1.hash);
    EXPECT_EQ(loaded[1].mode, e2.mode);
    EXPECT_EQ(loaded[1].path, e2.path);
    EXPECT_EQ(loaded[1].hash, e2.hash);
}

// 验证 upsert_index_entry 能够更新已有条目或插入新条目
TEST(IndexTest, UpsertEntryUpdatesOrAppends) {
    std::vector<minigit::IndexEntry> entries;

    minigit::IndexEntry e1;
    e1.mode = "100644";
    e1.path = "foo.txt";
    e1.hash = "1111111111111111111111111111111111111111";
    minigit::upsert_index_entry(entries, e1);
    ASSERT_EQ(entries.size(), 1U);

    minigit::IndexEntry e1_new = e1;
    e1_new.hash = "3333333333333333333333333333333333333333";
    minigit::upsert_index_entry(entries, e1_new);
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].hash, e1_new.hash);

    minigit::IndexEntry e2;
    e2.mode = "100644";
    e2.path = "bar.txt";
    e2.hash = "2222222222222222222222222222222222222222";
    minigit::upsert_index_entry(entries, e2);
    ASSERT_EQ(entries.size(), 2U);
}

