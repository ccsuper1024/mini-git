#pragma once

#include <string>
#include <vector>

#include "object_store.h"

// 本文件声明 commit 对象及 commit DAG 相关接口
namespace minigit {

/**
 * @brief 表示一个 Git 风格的 commit 对象。
 *
 * 包含指向 tree 的引用、父提交列表、作者/提交者信息以及提交信息。
 */
struct Commit {
    /// 顶层 tree 对象的 SHA-1 哈希（40 位十六进制字符串）。
    std::string tree;
    /// 父提交的哈希列表，支持多父（例如 merge 提交）。
    std::vector<std::string> parents;
    /// 作者信息，自由格式，通常包含姓名、邮箱和时间戳。
    std::string author;
    /// 提交者信息，自由格式，通常包含姓名、邮箱和时间戳。
    std::string committer;
    /// 提交说明，允许包含多行文本。
    std::string message;
};

/**
 * @brief 根据 Commit 结构生成完整的 commit 对象内容。
 *
 * 生成的二进制内容格式为：
 *   "commit <size>\\0"
 *   "tree <tree>\\n"
 *   ["parent <parent>\\n"...]
 *   "author <author>\\n"
 *   "committer <committer>\\n"
 *   "\\n"
 *   "<message>"
 *
 * @param commit 输入的 Commit 结构。
 * @return 含有头部和正文的完整 commit 对象二进制内容。
 */
std::string build_commit_object(const Commit& commit);

/**
 * @brief 解析 commit 对象内容为 Commit 结构。
 *
 * 支持两种输入形式：
 *   1. 含有 "commit <size>\\0" 头部的完整对象内容；
 *   2. 仅包含正文部分（从 "tree ..." 开始），例如 ObjectStore::read_object 的返回。
 *
 * @param content commit 对象的二进制内容或正文。
 * @param commit  输出参数，用于接收解析后的结果。
 * @return 解析成功返回 true，格式错误返回 false。
 */
bool parse_commit_object(const std::string& content, Commit& commit);

/**
 * @brief 使用对象存储写入 commit 对象并返回其哈希。
 *
 * 内部会先调用 build_commit_object 构造对象内容，再委托给 ObjectStore。
 *
 * @param store  对象存储实例。
 * @param commit 待写入的 commit 数据。
 * @return 写入后生成的 commit 哈希（40 位十六进制字符串）。
 */
std::string write_commit(ObjectStore& store, const Commit& commit);

}  // namespace minigit

