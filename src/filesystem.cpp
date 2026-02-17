#include "filesystem.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// 本文件实现基于 POSIX 接口的简单文件系统工具类
namespace {

// 判断字符是否为路径分隔符
bool is_separator(char c) {
    return c == '/' || c == '\\';
}

// 拼接两个路径字符串，尽量避免重复分隔符
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

// 若目标目录不存在则递归创建对应目录层级
bool mkdir_if_needed(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    if (::mkdir(path.c_str(), 0777) == 0) {
        return true;
    }

    if (errno == ENOENT) {
        std::size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos) {
            return false;
        }
        std::string parent = path.substr(0, pos);
        if (!mkdir_if_needed(parent)) {
            return false;
        }
        return ::mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
    }

    return errno == EEXIST;
}

}  // namespace

namespace minigit {

// 使用给定根目录构造文件系统对象
FileSystem::FileSystem(std::string root) : root_(std::move(root)) {}

// 返回当前文件系统的根目录
const std::string& FileSystem::root() const {
    return root_;
}

// 将相对路径转换为基于根目录的路径
std::string FileSystem::make_path(const std::string& relative) const {
    return join_paths(root_, relative);
}

// 确保给定相对路径对应的目录存在
bool FileSystem::ensure_directory(const std::string& relative) const {
    std::string full = make_path(relative);
    return mkdir_if_needed(full);
}

// 在指定相对路径写入数据，必要时先创建父目录
bool FileSystem::write_file(const std::string& relative,
                            const std::string& data) const {
    std::string full = make_path(relative);
    std::size_t pos = full.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string dir = full.substr(0, pos);
        if (!mkdir_if_needed(dir)) {
            return false;
        }
    }

    std::ofstream ofs(full.c_str(), std::ios::binary | std::ios::trunc);
    if (!ofs) {
        return false;
    }
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    return ofs.good();
}

// 从指定相对路径读取文件内容到 out
bool FileSystem::read_file(const std::string& relative,
                           std::string& out) const {
    std::string full = make_path(relative);
    std::ifstream ifs(full.c_str(), std::ios::binary);
    if (!ifs) {
        return false;
    }
    std::string buffer((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    out.swap(buffer);
    return true;
}

// 判断相对路径对应的文件或目录是否存在
bool FileSystem::exists(const std::string& relative) const {
    std::string full = make_path(relative);
    struct stat st;
    return stat(full.c_str(), &st) == 0;
}

}  // namespace minigit
