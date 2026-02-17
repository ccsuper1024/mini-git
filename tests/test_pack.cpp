#include "gtest/gtest.h"

#include "object_store.h"
#include "pack.h"
#include "zlib_utils.h"

TEST(PackfileTest, WriteAndReadPackfile) {
    char repo_tmpl[] = "/tmp/minigit_pack_repoXXXXXX";
    char* repo_dir_c = mkdtemp(repo_tmpl);
    ASSERT_NE(repo_dir_c, nullptr);
    std::string repo_dir(repo_dir_c);

    minigit::ObjectStore store(repo_dir + "/.minigit");
    minigit::FileSystem fs(repo_dir + "/.minigit");

    std::string h1 = store.store_blob("hello");
    std::string h2 = store.store_blob("world");

    ASSERT_TRUE(minigit::write_pack_file(fs, "objects/pack/test.mpk"));

    std::map<std::string, minigit::PackedEntry> entries;
    ASSERT_TRUE(minigit::read_pack_file(fs, "objects/pack/test.mpk", entries));
    ASSERT_FALSE(entries.empty());

    auto check_hash = [&](const std::string& h) {
        std::string loose_data;
        ASSERT_TRUE(store.read_object(h, loose_data));
        auto it = entries.find(h);
        ASSERT_NE(it, entries.end());
        std::string raw = minigit::zlib_decompress(it->second.compressed);
        std::size_t pos = raw.find('\0');
        ASSERT_NE(pos, std::string::npos);
        std::string body = raw.substr(pos + 1);
        ASSERT_EQ(body, loose_data);
    };

    check_hash(h1);
    check_hash(h2);
}

