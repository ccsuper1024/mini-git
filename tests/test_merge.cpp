#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unistd.h>
#include "commit.h"
#include "filesystem.h"
#include "index.h"
#include "object_store.h"
#include "tree.h"

static std::string store_blob(minigit::ObjectStore& store, const std::string& s) {
    return store.store_blob(s);
}
TEST(MergeBasicTest, ThreeWayMergeNoConflict) {
    char repo_tmpl[] = "/tmp/minigit_merge_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);
    minigit::ObjectStore store(repo_dir);
    minigit::FileSystem fs(repo_dir);
    //
    // base: a.txt=A
    std::vector<minigit::IndexEntry> base_idx;
    base_idx.push_back(minigit::IndexEntry{"100644", "a.txt", store_blob(store, "A")});
    std::string base_tree = minigit::write_tree_from_index(store, base_idx);
    minigit::Commit base_c;
    base_c.tree = base_tree;
    base_c.author = "U <u@e> 0 +0000";
    base_c.committer = "U <u@e> 0 +0000";
    base_c.message = "base";
    std::string base_hash = write_commit(store, base_c);
    //
    // ours: a.txt=AO (modified), b.txt=B (added)
    std::vector<minigit::IndexEntry> ours_idx;
    ours_idx.push_back(minigit::IndexEntry{"100644", "a.txt", store_blob(store, "AO")});
    ours_idx.push_back(minigit::IndexEntry{"100644", "b.txt", store_blob(store, "B")});
    std::string ours_tree = minigit::write_tree_from_index(store, ours_idx);
    minigit::Commit ours_c;
    ours_c.tree = ours_tree;
    ours_c.parents = {base_hash};
    ours_c.author = "U <u@e> 0 +0000";
    ours_c.committer = "U <u@e> 0 +0000";
    ours_c.message = "ours";
    std::string ours_hash = write_commit(store, ours_c);
    //
    // theirs: a.txt=A (unchanged), c.txt=C (added)
    std::vector<minigit::IndexEntry> theirs_idx;
    theirs_idx.push_back(minigit::IndexEntry{"100644", "a.txt", store_blob(store, "A")});
    theirs_idx.push_back(minigit::IndexEntry{"100644", "c.txt", store_blob(store, "C")});
    std::string theirs_tree = minigit::write_tree_from_index(store, theirs_idx);
    minigit::Commit theirs_c;
    theirs_c.tree = theirs_tree;
    theirs_c.parents = {base_hash};
    theirs_c.author = "U <u@e> 0 +0000";
    theirs_c.committer = "U <u@e> 0 +0000";
    theirs_c.message = "theirs";
    std::string theirs_hash = write_commit(store, theirs_c);
    //
    std::vector<minigit::IndexEntry> ibase, iours, itheirs;
    ASSERT_TRUE(minigit::flatten_tree_to_index(store, base_tree, ibase));
    ASSERT_TRUE(minigit::flatten_tree_to_index(store, ours_tree, iours));
    ASSERT_TRUE(minigit::flatten_tree_to_index(store, theirs_tree, itheirs));
    std::vector<minigit::IndexEntry> imerged;
    std::vector<std::string> conflicts;
    bool ok = minigit::three_way_merge_index(ibase, iours, itheirs, imerged, conflicts);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(conflicts.empty());
    //
    std::string merged_tree = minigit::write_tree_from_index(store, imerged);
    std::string tcontent;
    ASSERT_TRUE(store.read_object(merged_tree, tcontent));
    std::vector<minigit::TreeEntry> entries;
    ASSERT_TRUE(minigit::parse_tree_object(tcontent, entries));
    // merged should contain a.txt from ours, b.txt, c.txt
    bool has_a=false, has_b=false, has_c=false;
    for (const auto& e : entries) {
        if (e.mode == "100644") {
            if (e.name == "a.txt") has_a = true;
            if (e.name == "b.txt") has_b = true;
            if (e.name == "c.txt") has_c = true;
        }
    }
    EXPECT_TRUE(has_a);
    EXPECT_TRUE(has_b);
    EXPECT_TRUE(has_c);
}
#
TEST(MergeBasicTest, ThreeWayMergeConflictSameFileDifferent) {
    char repo_tmpl[] = "/tmp/minigit_merge_repo2XXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);
    minigit::ObjectStore store(repo_dir);
    //
    // base: a.txt=A
    std::vector<minigit::IndexEntry> base_idx;
    base_idx.push_back(minigit::IndexEntry{"100644", "a.txt", store.store_blob("A")});
    std::string base_tree = minigit::write_tree_from_index(store, base_idx);
    // ours: a.txt=AO
    std::vector<minigit::IndexEntry> ours_idx;
    ours_idx.push_back(minigit::IndexEntry{"100644", "a.txt", store.store_blob("AO")});
    std::string ours_tree = minigit::write_tree_from_index(store, ours_idx);
    // theirs: a.txt=AT
    std::vector<minigit::IndexEntry> theirs_idx;
    theirs_idx.push_back(minigit::IndexEntry{"100644", "a.txt", store.store_blob("AT")});
    std::string theirs_tree = minigit::write_tree_from_index(store, theirs_idx);
    //
    std::vector<minigit::IndexEntry> ibase, iours, itheirs;
    ASSERT_TRUE(minigit::flatten_tree_to_index(store, base_tree, ibase));
    ASSERT_TRUE(minigit::flatten_tree_to_index(store, ours_tree, iours));
    ASSERT_TRUE(minigit::flatten_tree_to_index(store, theirs_tree, itheirs));
    std::vector<minigit::IndexEntry> imerged;
    std::vector<std::string> conflicts;
    bool ok = minigit::three_way_merge_index(ibase, iours, itheirs, imerged, conflicts);
    ASSERT_FALSE(ok);
    ASSERT_FALSE(conflicts.empty());
}
