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

    std::time_t now = std::time(nullptr);
    long epoch = static_cast<long>(now);
    std::string author = "User <user@example.com> " + std::to_string(epoch) + " +0000";
    std::string committer = author;

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

// 程序入口，根据第一个参数选择执行的子命令
int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);

    if (argc < 2) {
        std::cerr << "usage: mini-git <command> [args]\n";
        std::cerr << "commands:\n";
        std::cerr << "  hash-object <file>\n";
        std::cerr << "  write-tree\n";
        std::cerr << "  add <file>\n";
        std::cerr << "  commit -m <message>\n";
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
