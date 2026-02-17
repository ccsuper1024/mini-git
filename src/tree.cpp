#include "tree.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

// 本文件实现 tree 对象的构造、解析以及目录快照写入逻辑
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

// 将 40 位十六进制哈希转换为 20 字节二进制形式
std::string hex_to_raw20(const std::string& hex) {
    if (hex.size() != 40U) {
        throw std::runtime_error("invalid sha1 hex length");
    }

    auto hex_to_nibble = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') {
            return static_cast<uint8_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<uint8_t>(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F') {
            return static_cast<uint8_t>(c - 'A' + 10);
        }
        throw std::runtime_error("invalid hex character");
    };

    std::string out;
    out.resize(20U);
    for (std::size_t i = 0; i < 20U; ++i) {
        uint8_t high = hex_to_nibble(hex[i * 2U]);
        uint8_t low = hex_to_nibble(hex[i * 2U + 1U]);
        out[i] = static_cast<char>((high << 4U) | low);
    }
    return out;
}

// 将 20 字节二进制哈希转换为 40 位十六进制字符串
std::string raw20_to_hex(const std::string& raw) {
    if (raw.size() != 20U) {
        throw std::runtime_error("invalid sha1 raw length");
    }

    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.resize(40U);
    for (std::size_t i = 0; i < 20U; ++i) {
        uint8_t v = static_cast<uint8_t>(raw[i]);
        out[i * 2U] = kHex[(v >> 4U) & 0x0FU];
        out[i * 2U + 1U] = kHex[v & 0x0FU];
    }
    return out;
}

// 递归遍历目录并构建 tree，对每个目录返回对应的 tree 哈希
std::string write_tree_recursive(minigit::ObjectStore& store,
                                 const std::string& dir_path) {
    DIR* dir = ::opendir(dir_path.c_str());
    if (!dir) {
        throw std::runtime_error("failed to open directory: " + dir_path);
    }

    std::vector<minigit::TreeEntry> entries;

    while (true) {
        errno = 0;
        dirent* entry = ::readdir(dir);
        if (!entry) {
            if (errno != 0) {
                int err = errno;
                ::closedir(dir);
                throw std::runtime_error("readdir failed with errno " +
                                         std::to_string(err));
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

        // 跳过 .minigit 仓库目录本身
        if (name == ".minigit") {
            continue;
        }

        std::string full_path = join_paths(dir_path, name);

        struct stat st;
        if (::lstat(full_path.c_str(), &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // 目录：递归构建子 tree
            std::string child_tree_hash =
                write_tree_recursive(store, full_path);
            minigit::TreeEntry e;
            e.mode = "40000";
            e.name = name;
            e.hash = child_tree_hash;
            entries.push_back(e);
        } else if (S_ISREG(st.st_mode)) {
            // 普通文件：写入 blob 对象
            std::FILE* fp = std::fopen(full_path.c_str(), "rb");
            if (!fp) {
                continue;
            }

            std::string data;
            char buffer[4096];
            while (true) {
                std::size_t n = std::fread(buffer, 1, sizeof(buffer), fp);
                if (n > 0) {
                    data.append(buffer, n);
                }
                if (n < sizeof(buffer)) {
                    if (std::ferror(fp)) {
                        data.clear();
                    }
                    break;
                }
            }
            std::fclose(fp);

            if (data.empty() && st.st_size > 0) {
                // 读取失败，跳过该文件
                continue;
            }

            std::string blob_hash = store.store_blob(data);
            minigit::TreeEntry e;
            e.mode = "100644";
            e.name = name;
            e.hash = blob_hash;
            entries.push_back(e);
        } else {
            // 暂不处理符号链接等其他类型
            continue;
        }
    }

    ::closedir(dir);

    // 为了保持结果稳定，对条目按名称排序
    std::sort(entries.begin(), entries.end(),
              [](const minigit::TreeEntry& a, const minigit::TreeEntry& b) {
                  return a.name < b.name;
              });

    std::string content = minigit::build_tree_object(entries);
    return store.store_tree(content);
}

}  // namespace

namespace minigit {

std::string build_tree_object(const std::vector<TreeEntry>& entries) {
    std::string body;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const TreeEntry& e = entries[i];
        body.append(e.mode);
        body.push_back(' ');
        body.append(e.name);
        body.push_back('\0');
        body.append(hex_to_raw20(e.hash));
    }

    std::string header = "tree " + std::to_string(body.size());
    header.push_back('\0');

    std::string content;
    content.reserve(header.size() + body.size());
    content.append(header);
    content.append(body);
    return content;
}

bool parse_tree_object(const std::string& content,
                       std::vector<TreeEntry>& entries) {
    entries.clear();

    std::size_t idx = 0;

    // 兼容两种输入：带头部的完整对象 或 仅条目正文
    std::size_t pos = content.find('\0');
    if (pos != std::string::npos) {
        std::string header = content.substr(0, pos);
        if (header.compare(0, 5, "tree ") == 0) {
            idx = pos + 1;
        } else {
            idx = 0;
        }
    }

    while (idx < content.size()) {
        // 解析 mode
        std::size_t space_pos = content.find(' ', idx);
        if (space_pos == std::string::npos) {
            return false;
        }
        std::string mode = content.substr(idx, space_pos - idx);

        // 解析 name
        std::size_t name_end = content.find('\0', space_pos + 1);
        if (name_end == std::string::npos) {
            return false;
        }
        std::string name =
            content.substr(space_pos + 1, name_end - (space_pos + 1));

        // 解析 20 字节哈希
        std::size_t hash_start = name_end + 1;
        if (hash_start + 20U > content.size()) {
            return false;
        }
        std::string raw_hash = content.substr(hash_start, 20U);
        std::string hex_hash = raw20_to_hex(raw_hash);

        TreeEntry entry;
        entry.mode = mode;
        entry.name = name;
        entry.hash = hex_hash;
        entries.push_back(entry);

        idx = hash_start + 20U;
    }

    return true;
}

std::string write_tree(ObjectStore& store, const std::string& root_dir) {
    return write_tree_recursive(store, root_dir);
}

}  // namespace minigit
