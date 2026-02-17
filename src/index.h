#pragma once

#include <string>
#include <vector>

#include "filesystem.h"

// 本文件声明 index（暂存区）数据结构及读写接口
namespace minigit {

/**
 * @brief 表示暂存区中的单个条目。
 *
 * 每个条目对应一个工作区路径及其当前暂存的对象哈希。
 */
struct IndexEntry {
    /// 文件模式，例如 "100644" 表示普通文件。
    std::string mode;
    /// 相对于工作区根目录的路径，例如 "src/main.cpp"。
    std::string path;
    /// 对应 blob 对象的 SHA-1 哈希（40 位十六进制字符串）。
    std::string hash;
};

/**
 * @brief 从仓库根目录下的 index 文件读取暂存区内容。
 *
 * 暂存区文件位于仓库根目录下的 "index" 路径，采用简单的文本格式：
 *   每行一个条目，格式为："<mode> <hash> <path>\\n"
 *
 * @param fs      指向仓库根目录的文件系统对象（例如根目录 ".minigit"）。
 * @param entries 输出参数，用于接收解析得到的条目列表。
 * @return 读取成功或文件不存在时返回 true，解析错误返回 false。
 */
bool read_index(const FileSystem& fs, std::vector<IndexEntry>& entries);

/**
 * @brief 将暂存区条目列表写回到仓库根目录下的 index 文件。
 *
 * 会覆盖原有 index 文件内容，如有需要会自动创建父目录。
 *
 * @param fs      指向仓库根目录的文件系统对象。
 * @param entries 待写入的条目列表。
 * @return 写入成功返回 true，否则返回 false。
 */
bool write_index(const FileSystem& fs, const std::vector<IndexEntry>& entries);

/**
 * @brief 在内存中的暂存区列表中插入或更新单个条目。
 *
 * 如果列表中已经存在相同 path 的条目，则覆盖其 mode 和 hash；
 * 否则将该条目追加到列表末尾。
 *
 * @param entries 暂存区条目列表。
 * @param entry   需要插入或更新的条目。
 */
void upsert_index_entry(std::vector<IndexEntry>& entries,
                        const IndexEntry& entry);

}  // namespace minigit

