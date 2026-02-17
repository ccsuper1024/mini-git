#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <dirent.h>

#include <spdlog/spdlog.h>

#include "checkout.h"
#include "commit.h"
#include "filesystem.h"
#include "index.h"
#include "object_store.h"
#include "refs.h"
#include "tree.h"
#include <set>
#include <map>

// 本文件实现 mini-git 命令行入口及子命令分发
namespace {

// 实现 hash-object 子命令，将文件内容存储为 blob 对象
int command_hash_object(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: mini-git hash-object <file>\n";
        return 1;
    }

    std::string path = argv[2];
    std::ifstream ifs(path.c_str(), std::ios::binary);
    if (!ifs) {
        std::cerr << "failed to open file: " << path << "\n";
        return 1;
    }

    std::string data((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());

    minigit::ObjectStore store(".minigit");
    std::string hash = store.store_blob(data);

    spdlog::info("stored blob {}", hash);
    std::cout << hash << "\n";
    return 0;
}

// 实现 write-tree 子命令，从当前工作目录构建目录快照
int command_write_tree(int /*argc*/, char** /*argv*/) {
    minigit::ObjectStore store(".minigit");
    std::string tree_hash = minigit::write_tree(store, ".");
    spdlog::info("write tree {}", tree_hash);
    std::cout << tree_hash << "\n";
    return 0;
}

// 实现 branch 子命令，列出或创建分支
int command_branch(int argc, char** argv) {
    minigit::FileSystem fs(".minigit");

    if (argc == 2) {
        minigit::Head head;
        bool has_head = minigit::read_head(fs, head);
        std::string current_ref;
        if (has_head && head.symbolic) {
            current_ref = head.target;
        }

        std::string heads_dir = fs.make_path("refs/heads");
        DIR* dir = ::opendir(heads_dir.c_str());
        if (!dir) {
            return 0;
        }

        std::vector<std::string> branches;
        struct dirent* entry = nullptr;
        while ((entry = ::readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") {
                continue;
            }
            branches.push_back(name);
        }
        ::closedir(dir);

        std::sort(branches.begin(), branches.end());

        for (std::size_t i = 0; i < branches.size(); ++i) {
            std::string refname = "refs/heads/" + branches[i];
            if (!current_ref.empty() && current_ref == refname) {
                std::cout << "* " << branches[i] << "\n";
            } else {
                std::cout << "  " << branches[i] << "\n";
            }
        }
        return 0;
    }

    if (argc == 3) {
        std::string name = argv[2];
        std::string refname = "refs/heads/" + name;

        minigit::Head head;
        if (!minigit::read_head(fs, head)) {
            std::cerr << "HEAD is not set\n";
            return 1;
        }

        std::string hash;
        if (head.symbolic) {
            if (!minigit::read_ref(fs, head.target, hash)) {
                std::cerr << "current branch has no commit\n";
                return 1;
            }
        } else {
            hash = head.target;
        }

        if (!minigit::update_ref(fs, refname, hash)) {
            std::cerr << "failed to update ref: " << refname << "\n";
            return 1;
        }

        std::cout << name << "\n";
        return 0;
    }

    std::cerr << "usage: mini-git branch [name]\n";
    return 1;
}

// 实现 symbolic-ref HEAD 子命令，设置 HEAD 指向指定分支
int command_symbolic_ref(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: mini-git symbolic-ref HEAD <ref>\n";
        return 1;
    }

    std::string target = argv[2];
    if (target != "HEAD") {
        std::cerr << "only HEAD symbolic ref is supported\n";
        return 1;
    }

    std::string refname = argv[3];
    minigit::FileSystem fs(".minigit");

    if (!minigit::set_head_symbolic(fs, refname)) {
        std::cerr << "failed to set HEAD\n";
        return 1;
    }

    return 0;
}

// 实现 status 子命令，显示当前分支或游离 HEAD 状态
int command_status(int /*argc*/, char** /*argv*/) {
    minigit::FileSystem fs(".minigit");
    minigit::Head head;

    if (!minigit::read_head(fs, head)) {
        std::cout << "HEAD is not set\n";
        return 0;
    }

    if (head.symbolic) {
        std::string refname = head.target;
        std::string branch = refname;
        const std::string prefix = "refs/heads/";
        if (branch.compare(0, prefix.size(), prefix) == 0) {
            branch = branch.substr(prefix.size());
        }

        std::string hash;
        bool has_hash = minigit::read_ref(fs, refname, hash);

        std::cout << "On branch " << branch << "\n";
        if (has_hash) {
            std::cout << "HEAD commit: " << hash << "\n";
        } else {
            std::cout << "HEAD commit: (no commit)\n";
        }
    } else {
        std::string hash = head.target;
        std::string short_hash = hash;
        if (short_hash.size() > 7U) {
            short_hash = short_hash.substr(0, 7U);
        }
        std::cout << "HEAD detached at " << short_hash << "\n";
    }

    return 0;
}

// 实现 checkout 子命令，根据 HEAD 或显式对象哈希重建工作区
int command_checkout(int argc, char** argv) {
    minigit::ObjectStore store(".minigit");

    std::string root_dir = ".";

    if (argc == 2) {
        bool ok = minigit::checkout_head(store, root_dir);
        if (!ok) {
            std::cerr << "checkout HEAD failed\n";
            return 1;
        }
        return 0;
    }

    if (argc == 3) {
        std::string arg = argv[2];
        if (arg.size() == 40U) {
            bool ok = minigit::checkout_commit(store, root_dir, arg);
            if (!ok) {
                ok = minigit::checkout_tree(store, root_dir, arg);
            }
            if (!ok) {
                std::cerr << "checkout " << arg << " failed\n";
                return 1;
            }
            return 0;
        }

        std::string refname = "refs/heads/" + arg;
        minigit::FileSystem fs(".minigit");
        std::string hash;
        if (!minigit::read_ref(fs, refname, hash)) {
            std::cerr << "unknown revision: " << arg << "\n";
            return 1;
        }

        if (!minigit::checkout_commit(store, root_dir, hash)) {
            std::cerr << "checkout " << arg << " failed\n";
            return 1;
        }

        if (!minigit::set_head_symbolic(fs, refname)) {
            std::cerr << "failed to update HEAD\n";
            return 1;
        }

        return 0;
    }

    std::cerr << "usage: mini-git checkout [<branch>|<hash>]\n";
    return 1;
}

// 实现 add 子命令，将指定路径的文件加入暂存区
int command_add(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: mini-git add <file>\n";
        return 1;
    }

    std::string path = argv[2];

    std::ifstream ifs(path.c_str(), std::ios::binary);
    if (!ifs) {
        std::cerr << "failed to open file: " << path << "\n";
        return 1;
    }

    std::string data((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());

    minigit::ObjectStore store(".minigit");
    std::string hash = store.store_blob(data);

    minigit::FileSystem fs(".minigit");
    std::vector<minigit::IndexEntry> entries;
    if (!minigit::read_index(fs, entries)) {
        std::cerr << "failed to read index\n";
        return 1;
    }

    minigit::IndexEntry e;
    e.mode = "100644";
    e.path = path;
    e.hash = hash;
    minigit::upsert_index_entry(entries, e);

    if (!minigit::write_index(fs, entries)) {
        std::cerr << "failed to write index\n";
        return 1;
    }

    return 0;
}

// 实现 commit 子命令：从 index 构建 tree，生成 commit 并更新当前分支
int command_commit(int argc, char** argv) {
    if (argc < 4 || std::string(argv[2]) != "-m") {
        std::cerr << "usage: mini-git commit -m <message>\n";
        return 1;
    }
    std::string message = argv[3];

    minigit::FileSystem fs(".minigit");
    std::vector<minigit::IndexEntry> entries;
    if (!minigit::read_index(fs, entries)) {
        std::cerr << "failed to read index\n";
        return 1;
    }

    minigit::ObjectStore store(".minigit");
    std::string tree_hash = minigit::write_tree_from_index(store, entries);

    minigit::Head head;
    bool has_head = minigit::read_head(fs, head);
    std::vector<std::string> parents;
    if (has_head) {
        if (head.symbolic) {
            std::string parent_hash;
            if (minigit::read_ref(fs, head.target, parent_hash)) {
                parents.push_back(parent_hash);
            }
        } else {
            parents.push_back(head.target);
        }
    }

    std::string author = minigit::build_identity_from_env("GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_AUTHOR_DATE");
    std::string committer = minigit::build_identity_from_env("GIT_COMMITTER_NAME", "GIT_COMMITTER_EMAIL", "GIT_COMMITTER_DATE");

    minigit::Commit c;
    c.tree = tree_hash;
    c.parents = parents;
    c.author = author;
    c.committer = committer;
    c.message = message;

    std::string commit_hash = minigit::write_commit(store, c);

    if (has_head && head.symbolic) {
        if (!minigit::update_ref(fs, head.target, commit_hash)) {
            std::cerr << "failed to update ref: " << head.target << "\n";
            return 1;
        }
    } else {
        if (!minigit::set_head_detached(fs, commit_hash)) {
            std::cerr << "failed to update HEAD\n";
            return 1;
        }
    }

    std::cout << commit_hash << "\n";
    return 0;
}
}  // namespace
// 实现 merge 子命令：与指定提交或分支进行三方合并并生成 merge commit
static bool read_commit(minigit::ObjectStore& store, const std::string& h, minigit::Commit& c) {
    std::string body;
    if (!store.read_object(h, body)) return false;
    return minigit::parse_commit_object(body, c);
}

static std::string resolve_target_commit_hash(minigit::FileSystem& fs, const std::string& arg) {
    if (arg.size() == 40U) {
        return arg;
    }
    std::string refname = "refs/heads/" + arg;
    std::string hash;
    if (minigit::read_ref(fs, refname, hash)) {
        return hash;
    }
    return std::string();
}

static std::string find_common_ancestor(minigit::ObjectStore& store,
                                        const std::string& a,
                                        const std::string& b) {
    std::vector<std::string> queue;
    std::vector<std::string> aq;
    aq.push_back(a);
    std::set<std::string> Aanc;
    while (!aq.empty()) {
        std::string x = aq.back();
        aq.pop_back();
        if (Aanc.count(x)) continue;
        Aanc.insert(x);
        minigit::Commit c;
        if (!read_commit(store, x, c)) continue;
        for (const auto& p : c.parents) {
            aq.push_back(p);
        }
    }
    queue.push_back(b);
    std::set<std::string> visited;
    while (!queue.empty()) {
        std::string y = queue.back();
        queue.pop_back();
        if (visited.count(y)) continue;
        visited.insert(y);
        if (Aanc.count(y)) return y;
        minigit::Commit c;
        if (!read_commit(store, y, c)) continue;
        for (const auto& p : c.parents) {
            queue.push_back(p);
        }
    }
    return std::string();
}

static bool is_ancestor(minigit::ObjectStore& store,
                        const std::string& anc,
                        const std::string& desc) {
    std::vector<std::string> stack;
    stack.push_back(desc);
    std::set<std::string> visited;
    while (!stack.empty()) {
        std::string x = stack.back();
        stack.pop_back();
        if (x == anc) return true;
        if (visited.count(x)) continue;
        visited.insert(x);
        minigit::Commit c;
        if (!read_commit(store, x, c)) continue;
        for (const auto& p : c.parents) {
            stack.push_back(p);
        }
    }
    return false;
}

int command_merge(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: mini-git merge [--no-ff|--ff-only] [--ours|--theirs] <commit|branch>\n";
        return 1;
    }
    bool no_ff = false;
    bool ff_only = false;
    enum { RES_NONE, RES_OURS, RES_THEIRS } resolve = RES_NONE;
    std::string target;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--no-ff") {
            no_ff = true;
        } else if (a == "--ff-only") {
            ff_only = true;
        } else if (a == "--ours") {
            resolve = RES_OURS;
        } else if (a == "--theirs") {
            resolve = RES_THEIRS;
        } else {
            target = a;
        }
    }
    if (target.empty()) {
        std::cerr << "merge: missing <commit|branch>\n";
        return 1;
    }
    minigit::FileSystem fs(".minigit");
    minigit::ObjectStore store(".minigit");
    minigit::Head head;
    if (!minigit::read_head(fs, head)) {
        std::cerr << "HEAD is not set\n";
        return 1;
    }
    std::string ours_commit;
    if (head.symbolic) {
        if (!minigit::read_ref(fs, head.target, ours_commit) || ours_commit.empty()) {
            std::cerr << "current branch has no commit\n";
            return 1;
        }
    } else {
        ours_commit = head.target;
    }
    std::string theirs_commit = resolve_target_commit_hash(fs, target);
    if (theirs_commit.empty()) {
        std::cerr << "unknown revision: " << target << "\n";
        return 1;
    }
    minigit::Commit oc, tc;
    if (!read_commit(store, ours_commit, oc) || !read_commit(store, theirs_commit, tc)) {
        std::cerr << "failed to read commits\n";
        return 1;
    }
    // Fast-forward checks
    bool can_ff = is_ancestor(store, ours_commit, theirs_commit);
    if (can_ff && !no_ff) {
        if (head.symbolic) {
            if (!minigit::update_ref(fs, head.target, theirs_commit)) {
                std::cerr << "failed to update ref: " << head.target << "\n";
                return 1;
            }
        } else {
            if (!minigit::set_head_detached(fs, theirs_commit)) {
                std::cerr << "failed to update HEAD\n";
                return 1;
            }
        }
        bool ok_co = minigit::checkout_commit(store, ".", theirs_commit);
        if (!ok_co) {
            std::cerr << "checkout merged result failed\n";
            return 1;
        }
        std::cout << theirs_commit << "\n";
        return 0;
    }
    if (ff_only && !can_ff) {
        std::cerr << "merge: not fast-forward\n";
        return 1;
    }
    std::string base = find_common_ancestor(store, ours_commit, theirs_commit);
    std::vector<minigit::IndexEntry> ibase, iours, itheirs;
    if (!base.empty()) {
        minigit::Commit bc;
        if (!read_commit(store, base, bc)) {
            std::cerr << "failed to read base commit\n";
            return 1;
        }
        if (!minigit::flatten_tree_to_index(store, bc.tree, ibase)) {
            std::cerr << "failed to flatten base tree\n";
            return 1;
        }
    }
    if (!minigit::flatten_tree_to_index(store, oc.tree, iours)) {
        std::cerr << "failed to flatten ours tree\n";
        return 1;
    }
    if (!minigit::flatten_tree_to_index(store, tc.tree, itheirs)) {
        std::cerr << "failed to flatten theirs tree\n";
        return 1;
    }
    std::vector<minigit::IndexEntry> imerged;
    std::vector<std::string> conflicts;
    bool ok = minigit::three_way_merge_index(ibase, iours, itheirs, imerged, conflicts);
    if (!ok) {
        if (resolve == RES_NONE) {
            std::cerr << "merge conflicts:\n";
            for (const auto& p : conflicts) {
                std::cerr << "  " << p << "\n";
            }
            return 1;
        }
        // apply quick resolution strategy
        auto to_map = [](const std::vector<minigit::IndexEntry>& xs) {
            std::map<std::string, minigit::IndexEntry> m;
            for (const auto& x : xs) m[x.path] = x;
            return m;
        };
        auto Mo = to_map(iours);
        auto Mt = to_map(itheirs);
        for (const auto& p : conflicts) {
            if (resolve == RES_OURS && Mo.count(p)) {
                imerged.push_back(Mo[p]);
            } else if (resolve == RES_THEIRS && Mt.count(p)) {
                imerged.push_back(Mt[p]);
            }
        }
        conflicts.clear();
    }
    std::string merged_tree = minigit::write_tree_from_index(store, imerged);
    std::string author = minigit::build_identity_from_env("GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_AUTHOR_DATE");
    std::string committer = minigit::build_identity_from_env("GIT_COMMITTER_NAME", "GIT_COMMITTER_EMAIL", "GIT_COMMITTER_DATE");
    minigit::Commit mc;
    mc.tree = merged_tree;
    mc.parents = {ours_commit, theirs_commit};
    mc.author = author;
    mc.committer = committer;
    mc.message = "merge " + target;
    std::string mh = minigit::write_commit(store, mc);
    if (head.symbolic) {
        if (!minigit::update_ref(fs, head.target, mh)) {
            std::cerr << "failed to update ref: " << head.target << "\n";
            return 1;
        }
    } else {
        if (!minigit::set_head_detached(fs, mh)) {
            std::cerr << "failed to update HEAD\n";
            return 1;
        }
    }
    bool ok_co = minigit::checkout_commit(store, ".", mh);
    if (!ok_co) {
        std::cerr << "checkout merged result failed\n";
        return 1;
    }
    std::cout << mh << "\n";
    return 0;
}

// 程序入口，根据第一个参数选择执行的子命令
int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_level(spdlog::level::info);

    if (argc < 2) {
        std::cerr << "usage: mini-git <command> [args]\n";
        std::cerr << "commands:\n";
        std::cerr << "  hash-object <file>\n";
        std::cerr << "  write-tree\n";
        std::cerr << "  add <file>\n";
        std::cerr << "  commit -m <message>\n";
        std::cerr << "  merge <commit|branch>\n";
        std::cerr << "  branch [name]\n";
        std::cerr << "  symbolic-ref HEAD <ref>\n";
        std::cerr << "  status\n";
        std::cerr << "  checkout [<branch>|<hash>]\n";
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "hash-object") {
        return command_hash_object(argc, argv);
    }
    if (cmd == "write-tree") {
        return command_write_tree(argc, argv);
    }
    if (cmd == "add") {
        return command_add(argc, argv);
    }
    if (cmd == "commit") {
        return command_commit(argc, argv);
    }
    if (cmd == "merge") {
        return command_merge(argc, argv);
    }
    if (cmd == "branch") {
        return command_branch(argc, argv);
    }
    if (cmd == "symbolic-ref") {
        return command_symbolic_ref(argc, argv);
    }
    if (cmd == "status") {
        return command_status(argc, argv);
    }
    if (cmd == "checkout") {
        return command_checkout(argc, argv);
    }

    std::cerr << "unknown command: " << cmd << "\n";
    return 1;
}
