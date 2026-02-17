#include <gtest/gtest.h>
#include <cstdlib>
#include "commit.h"
#include "object_store.h"
TEST(IdentityEnvTest, BuildFromEnvOverridesDefaults) {
    setenv("GIT_AUTHOR_NAME", "Alice", 1);
    setenv("GIT_AUTHOR_EMAIL", "alice@example.com", 1);
    setenv("GIT_AUTHOR_DATE", "1700000000 +0800", 1);
    std::string author = minigit::build_identity_from_env("GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_AUTHOR_DATE");
    ASSERT_NE(author.find("Alice <alice@example.com> 1700000000 +0800"), std::string::npos);
}
TEST(IdentityEnvTest, CommitUsesIdentityString) {
    unsetenv("GIT_COMMITTER_NAME");
    unsetenv("GIT_COMMITTER_EMAIL");
    unsetenv("GIT_COMMITTER_DATE");
    minigit::ObjectStore store(".minigit");
    minigit::Commit c;
    c.tree = std::string(40, 'a');
    c.message = "msg";
    c.author = minigit::build_identity_from_env("GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_AUTHOR_DATE");
    c.committer = minigit::build_identity_from_env("GIT_COMMITTER_NAME", "GIT_COMMITTER_EMAIL", "GIT_COMMITTER_DATE");
    std::string content = minigit::build_commit_object(c);
    ASSERT_NE(content.find("author " + c.author + "\n"), std::string::npos);
    ASSERT_NE(content.find("committer " + c.committer + "\n"), std::string::npos);
}
