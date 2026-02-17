#pragma once

#include <string>

#include "object_store.h"

// 本文件声明 checkout 相关接口，用于根据 tree 或 commit 重建工作区
namespace minigit {

/**
 * @brief 根据给定 tree 对象哈希重建工作区。
 *
 * 会在工作目录下创建或覆盖文件和目录，使其与指定 tree 快照一致，
 * 同时跳过 .minigit 仓库目录本身。
 *
 * 当前实现采用“清空再重建”的策略：
 *   1. 删除工作目录下除 .minigit 以外的所有文件和子目录；
 *   2. 递归展开 tree，将 blob 写回文件系统。
 *
 * @param store     对象存储实例，用于读取 tree 与 blob 对象内容。
 * @param root_dir  工作区根目录路径，例如 "."。
 * @param tree_hash 顶层 tree 对象的 SHA-1 哈希。
 * @return 操作成功返回 true，否则返回 false。
 */
bool checkout_tree(ObjectStore& store,
                   const std::string& root_dir,
                   const std::string& tree_hash);

/**
 * @brief 根据 commit 哈希重建工作区。
 *
 * 会先读取 commit 对象并解析出顶层 tree 哈希，然后委托给
 * checkout_tree 完成实际的工作区重建。
 *
 * @param store       对象存储实例。
 * @param root_dir    工作区根目录路径，例如 "."。
 * @param commit_hash 目标 commit 的 SHA-1 哈希。
 * @return 操作成功返回 true，否则返回 false。
 */
bool checkout_commit(ObjectStore& store,
                     const std::string& root_dir,
                     const std::string& commit_hash);

/**
 * @brief 根据当前 HEAD 所指向的分支或提交重建工作区。
 *
 * 解析 .minigit/HEAD 状态：
 *   - 若为符号引用（refs/heads/<name>），则读取对应 ref 文件中的提交哈希；
 *   - 若为游离 HEAD，则直接使用 HEAD 中记录的提交哈希。
 *
 * 随后调用 checkout_commit 完成实际重建。
 *
 * @param store    对象存储实例。
 * @param root_dir 工作区根目录路径，例如 "."。
 * @return 操作成功返回 true，否则返回 false。
 */
bool checkout_head(ObjectStore& store, const std::string& root_dir);

}  // namespace minigit

