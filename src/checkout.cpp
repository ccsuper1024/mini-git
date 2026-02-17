#include "checkout.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "commit.h"
#include "filesystem.h"
#include "refs.h"
#include "tree.h"

// 本文件实现基于 tree/commit/HEAD 的工作区重建逻辑
namespace {

// 判断是否为路径分隔符
bool is_separator(char c) {
    return c == '/' || c == '\\';
}

// 拼接两个路径片段，避免重复分隔符
std::string join_paths(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    if (is_separator(a.back())) {
        if (is_separator(b.front())) {
            return a + b.substr(1);
        }
        return a + b;
    }
    if (is_separator(b.front())) {
        return a + b;
    }
    return a + "/" + b;
}

// 递归删除目录内容，保留根目录本身
bool remove_tree_except_root(const std::string& root_path) {
    DIR* dir = ::opendir(root_path.c_str());
    if (!dir) {
        return false;
    }

    while (true) {
        errno = 0;
        dirent* entry = ::readdir(dir);
        if (!entry) {
            if (errno != 0) {
                int err = errno;
                ::closedir(dir);
                (void)err;
                return false;
            }
            break;
        }

        const char* name_cstr = entry->d_name;
        if (!name_cstr) {
            continue;
        }

        std::string name(name_cstr);
        if (name == "." || name == "..") {
            continue;
        }

        if (name == ".minigit") {
            continue;
        }

        std::string full_path = join_paths(root_path, name);

        struct stat st;
        if (::lstat(full_path.c_str(), &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!remove_tree_except_root(full_path)) {
                ::closedir(dir);
                return false;
            }
            if (::rmdir(full_path.c_str()) != 0) {
                ::closedir(dir);
                return false;
            }
        } else {
            if (::unlink(full_path.c_str()) != 0) {
                ::closedir(dir);
                return false;
            }
        }
    }

    ::closedir(dir);
    return true;
}

// 根据 tree 条目递归写回目录结构
bool restore_tree(minigit::ObjectStore& store,
                  const std::string& root_dir,
                  const std::string& tree_hash) {
    std::string tree_content;
    if (!store.read_object(tree_hash, tree_content)) {
        return false;
    }

    std::vector<minigit::TreeEntry> entries;
    if (!minigit::parse_tree_object(tree_content, entries)) {
        return false;
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const minigit::TreeEntry& e = entries[i];
        std::string path = join_paths(root_dir, e.name);

        if (e.mode == "40000") {
            if (::mkdir(path.c_str(), 0777) != 0) {
                struct stat st;
                if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                    return false;
                }
            }
            if (!restore_tree(store, path, e.hash)) {
                return false;
            }
        } else if (e.mode == "100644") {
            std::string blob_data;
            if (!store.read_object(e.hash, blob_data)) {
                return false;
            }
            std::FILE* fp = std::fopen(path.c_str(), "wb");
            if (!fp) {
                return false;
            }
            if (!blob_data.empty()) {
                std::size_t written =
                    std::fwrite(blob_data.data(), 1, blob_data.size(), fp);
                if (written != blob_data.size()) {
                    std::fclose(fp);
                    return false;
                }
            }
            std::fclose(fp);
        } else {
            // 暂不处理其他文件模式
            continue;
        }
    }

    return true;
}

}  // namespace

namespace minigit {

bool checkout_tree(ObjectStore& store,
                   const std::string& root_dir,
                   const std::string& tree_hash) {
    if (!remove_tree_except_root(root_dir)) {
        return false;
    }
    return restore_tree(store, root_dir, tree_hash);
}

bool checkout_commit(ObjectStore& store,
                     const std::string& root_dir,
                     const std::string& commit_hash) {
    std::string content;
    if (!store.read_object(commit_hash, content)) {
        return false;
    }

    Commit commit;
    if (!parse_commit_object(content, commit)) {
        return false;
    }

    if (commit.tree.empty()) {
        return false;
    }

    return checkout_tree(store, root_dir, commit.tree);
}

bool checkout_head(ObjectStore& store, const std::string& root_dir) {
    FileSystem fs(".minigit");
    Head head;
    if (!read_head(fs, head)) {
        return false;
    }

    std::string commit_hash;
    if (head.symbolic) {
        if (!read_ref(fs, head.target, commit_hash)) {
            return false;
        }
    } else {
        commit_hash = head.target;
    }

    if (commit_hash.empty()) {
        return false;
    }

    return checkout_commit(store, root_dir, commit_hash);
}

}  // namespace minigit

