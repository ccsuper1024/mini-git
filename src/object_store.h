#pragma once

#include <string>

#include "filesystem.h"

// 本文件声明对象存储类，用于管理 .minigit/objects 下的 Git 对象
namespace minigit {

/**
 * @brief Git 对象存储抽象。
 *
 * 负责在给定仓库根目录下创建 objects 目录，并将对象内容压缩后
 * 以 "objects/aa/bb..." 的形式持久化，同时提供按哈希读取的能力。
 */
class ObjectStore {
public:
    /**
     * @brief 使用给定仓库根目录构造对象存储。
     *
     * 构造函数会确保 <root>/objects 目录存在。
     *
     * @param root 仓库根目录路径，例如 ".minigit"。
     */
    explicit ObjectStore(const std::string& root);

    /**
     * @brief 将原始数据存储为 blob 对象。
     *
     * 内部会按照 Git 格式构造 blob 对象内容并压缩写入磁盘。
     *
     * @param data 原始 blob 数据。
     * @return 对象内容的 SHA-1 哈希（40 位十六进制字符串）。
     */
    std::string store_blob(const std::string& data);

    /**
     * @brief 根据对象哈希读取对象内容。
     *
     * 读取并解压后，自动去除 "type size\\0" 头部，仅返回正文部分。
     *
     * @param hash     对象的 SHA-1 哈希（40 位十六进制字符串）。
     * @param out_data 输出参数，用于接收对象正文。
     * @return 读取成功返回 true，否则返回 false。
     */
    bool read_object(const std::string& hash, std::string& out_data);

    /**
     * @brief 将完整的 tree 对象内容写入存储。
     *
     * 输入内容应包含 "tree <size>\\0" 头部及后续条目，哈希计算与 blob 一致。
     *
     * @param content 完整的 tree 对象二进制内容。
     * @return 对象内容的 SHA-1 哈希（40 位十六进制字符串）。
     */
    std::string store_tree(const std::string& content);

    /**
     * @brief 将完整的 commit 对象内容写入存储。
     *
     * 输入内容应包含 "commit <size>\\0" 头部及后续字段和消息体，
     * 哈希计算规则与 blob/tree 保持一致。
     *
     * @param content 完整的 commit 对象二进制内容。
     * @return 对象内容的 SHA-1 哈希（40 位十六进制字符串）。
     */
    std::string store_commit(const std::string& content);

private:
    FileSystem fs_;
    std::string objects_dir_;
};

}  // namespace minigit
