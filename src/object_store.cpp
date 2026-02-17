#include "object_store.h"

#include <stdexcept>

#include "blob.h"
#include "hash.h"
#include "zlib_utils.h"

// 本文件实现对象存储逻辑，将各类 Git 对象持久化到磁盘
namespace minigit {

// 使用给定仓库根目录构造对象存储，并确保 objects 目录存在
ObjectStore::ObjectStore(const std::string& root)
    : fs_(root), objects_dir_("objects") {
    fs_.ensure_directory(objects_dir_);
}

// 内部辅助函数：根据对象内容计算哈希并落盘
static std::string store_raw_object(FileSystem& fs, const std::string& objects_dir,
                                    const std::string& content) {
    std::string compressed = zlib_compress(content);
    std::string hash = sha1_hex(content);

    if (hash.size() < 3) {
        throw std::runtime_error("invalid hash");
    }

    std::string dir = objects_dir + "/" + hash.substr(0, 2);
    std::string file = hash.substr(2);

    fs.ensure_directory(dir);
    std::string path = dir + "/" + file;

    if (!fs.exists(path)) {
        if (!fs.write_file(path, compressed)) {
            throw std::runtime_error("failed to write object file");
        }
    }

    return hash;
}

// 将原始数据包装成 blob 对象并压缩写入磁盘，返回对象哈希
std::string ObjectStore::store_blob(const std::string& data) {
    std::string content = build_blob_object(data);
    return store_raw_object(fs_, objects_dir_, content);
}

// 根据对象哈希读取对象内容，解析头部后将正文写入 out_data
bool ObjectStore::read_object(const std::string& hash, std::string& out_data) {
    if (hash.size() < 3) {
        return false;
    }

    std::string dir = objects_dir_ + "/" + hash.substr(0, 2);
    std::string file = hash.substr(2);
    std::string path = dir + "/" + file;

    std::string compressed;
    if (!fs_.read_file(path, compressed)) {
        return false;
    }

    std::string content = zlib_decompress(compressed);
    std::size_t pos = content.find('\0');
    if (pos == std::string::npos) {
        return false;
    }

    out_data = content.substr(pos + 1);
    return true;
}

// 将完整的 tree 对象内容压缩写入磁盘，返回对象哈希
std::string ObjectStore::store_tree(const std::string& content) {
    return store_raw_object(fs_, objects_dir_, content);
}

// 将完整的 commit 对象内容压缩写入磁盘，返回对象哈希
std::string ObjectStore::store_commit(const std::string& content) {
    return store_raw_object(fs_, objects_dir_, content);
}

}  // namespace minigit
