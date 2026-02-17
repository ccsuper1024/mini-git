#pragma once

#include <string>
#include <vector>

#include "object_store.h"
#include "index.h"

// 本文件声明 tree 对象及目录快照相关接口
namespace minigit {

/**
 * @brief 表示 tree 对象中的单个条目。
 *
 * 一个条目对应一个文件或子目录，包含文件模式、名称和目标对象哈希。
 */
struct TreeEntry {
    /// 文件模式，例如 "100644" 表示普通文件，"40000" 表示目录。
    std::string mode;
    /// 条目名称，对应文件名或目录名。
    std::string name;
    /// 目标对象的 SHA-1 哈希（40 位十六进制字符串）。
    std::string hash;
};

/**
 * @brief 根据给定的条目列表构造 tree 对象内容。
 *
 * 生成的二进制内容格式为：
 *   "tree <size>\\0" + N 个条目，其中每个条目的格式为：
 *   "<mode> <name>\\0<20 字节二进制哈希>"。
 *
 * @param entries tree 中包含的条目列表。
 * @return 完整的 tree 对象二进制内容。
 */
std::string build_tree_object(const std::vector<TreeEntry>& entries);

/**
 * @brief 解析 tree 对象内容为条目列表。
 *
 * 支持两种输入形式：
 *   1. 含有 "tree <size>\\0" 头部的完整对象内容；
 *   2. 仅包含条目部分（从 "<mode> <name>\\0..." 开始），
 *      例如 ObjectStore::read_object 的返回。
 *
 * @param content tree 对象的二进制内容或条目正文。
 * @param entries 输出参数，用于接收解析得到的条目列表。
 * @return 解析成功返回 true，格式错误返回 false。
 */
bool parse_tree_object(const std::string& content,
                       std::vector<TreeEntry>& entries);

/**
 * @brief 从指定工作目录构建目录快照并写入对象存储。
 *
 * 递归遍历工作目录，将普通文件写入为 blob 对象，将目录结构写入为 tree 对象，
 * 最终返回顶层目录对应的 tree 哈希。
 *
 * @param store    对象存储实例，用于写入 blob/tree 对象。
 * @param root_dir 需要快照的工作目录路径。
 * @return 顶层 tree 对象的 SHA-1 哈希（40 位十六进制字符串）。
 * @throws std::runtime_error 当文件访问或对象写入发生致命错误时抛出异常。
 */
std::string write_tree(ObjectStore& store, const std::string& root_dir);

/**
 * @brief 根据 index 条目构建顶层 tree 并写入对象存储。
 *
 * 会解析每个条目的 path，按目录层级递归构建子 tree，文件以 blob 哈希填充，
 * 目录以子 tree 哈希填充，最终返回顶层目录对应的 tree 哈希。
 *
 * @param store   对象存储实例。
 * @param entries 暂存区条目列表。
 * @return 顶层 tree 对象的 SHA-1 哈希（40 位十六进制字符串）。
 */
std::string write_tree_from_index(ObjectStore& store,
                                  const std::vector<IndexEntry>& entries);

bool flatten_tree_to_index(ObjectStore& store,
                           const std::string& tree_hash,
                           std::vector<IndexEntry>& entries);

bool three_way_merge_index(const std::vector<IndexEntry>& base,
                           const std::vector<IndexEntry>& ours,
                           const std::vector<IndexEntry>& theirs,
                           std::vector<IndexEntry>& merged,
                           std::vector<std::string>& conflicts);

}  // namespace minigit
