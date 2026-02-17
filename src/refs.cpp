#include "refs.h"

#include <cstddef>

// 本文件实现 refs 与 HEAD 管理的具体逻辑
namespace {

// 去除字符串末尾的 '\n' 或 '\r\n'
std::string rstrip_newline(const std::string& s) {
    if (s.empty()) {
        return s;
    }
    std::size_t end = s.size();
    while (end > 0 && (s[end - 1] == '\n' || s[end - 1] == '\r')) {
        --end;
    }
    if (end == s.size()) {
        return s;
    }
    return s.substr(0, end);
}

}  // namespace

namespace minigit {

bool set_head_symbolic(const FileSystem& fs, const std::string& refname) {
    std::string content = "ref: " + refname + "\n";
    return fs.write_file("HEAD", content);
}

bool set_head_detached(const FileSystem& fs, const std::string& hash) {
    std::string content = hash;
    content.push_back('\n');
    return fs.write_file("HEAD", content);
}

bool read_head(const FileSystem& fs, Head& head) {
    std::string data;
    if (!fs.read_file("HEAD", data)) {
        return false;
    }

    data = rstrip_newline(data);
    if (data.compare(0, 5, "ref: ") == 0) {
        head.symbolic = true;
        head.target = data.substr(5);
        return !head.target.empty();
    }

    if (data.empty()) {
        return false;
    }

    head.symbolic = false;
    head.target = data;
    return true;
}

bool update_ref(const FileSystem& fs,
                const std::string& refname,
                const std::string& hash) {
    // 需要确保上级目录存在
    std::size_t pos = refname.rfind('/');
    if (pos != std::string::npos) {
        std::string dir = refname.substr(0, pos);
        if (!fs.ensure_directory(dir)) {
            return false;
        }
    }

    std::string content = hash;
    content.push_back('\n');
    return fs.write_file(refname, content);
}

bool read_ref(const FileSystem& fs,
              const std::string& refname,
              std::string& hash) {
    std::string data;
    if (!fs.read_file(refname, data)) {
        return false;
    }
    data = rstrip_newline(data);
    if (data.empty()) {
        return false;
    }
    hash = data;
    return true;
}

}  // namespace minigit

