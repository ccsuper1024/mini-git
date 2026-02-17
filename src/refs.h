#pragma once

#include <string>

#include "filesystem.h"

// 本文件声明 refs 与 HEAD 管理接口，实现简单的分支系统
namespace minigit {

/**
 * @brief 表示 HEAD 的当前状态。
 *
 * 当 symbolic 为 true 时，target 表示一个 ref 路径（例如 "refs/heads/master"）；
 * 当 symbolic 为 false 时，target 表示一个具体的提交哈希（40 位十六进制字符串）。
 */
struct Head {
    /// 是否为符号引用（指向某个 ref 文件）。
    bool symbolic;
    /// 目标 ref 路径或提交哈希。
    std::string target;
};

/**
 * @brief 将 HEAD 设置为符号引用，指向指定 ref。
 *
 * HEAD 文件内容将被写为： "ref: <refname>\\n"。
 *
 * @param fs      仓库根目录对应的文件系统对象（例如根目录 ".minigit"）。
 * @param refname 目标 ref 路径，例如 "refs/heads/master"。
 * @return 写入成功返回 true，否则返回 false。
 */
bool set_head_symbolic(const FileSystem& fs, const std::string& refname);

/**
 * @brief 将 HEAD 设置为游离状态，直接指向指定提交哈希。
 *
 * HEAD 文件内容将被写为： "<hash>\\n"。
 *
 * @param fs   仓库根目录对应的文件系统对象。
 * @param hash 目标提交哈希（40 位十六进制字符串）。
 * @return 写入成功返回 true，否则返回 false。
 */
bool set_head_detached(const FileSystem& fs, const std::string& hash);

/**
 * @brief 读取当前 HEAD 状态。
 *
 * 支持两种格式：
 *   1. "ref: <refname>\\n" 形式的符号引用；
 *   2. "<hash>\\n" 形式的游离 HEAD。
 *
 * @param fs   仓库根目录对应的文件系统对象。
 * @param head 输出参数，用于接收解析得到的 HEAD 状态。
 * @return HEAD 存在且格式合法时返回 true，否则返回 false。
 */
bool read_head(const FileSystem& fs, Head& head);

/**
 * @brief 更新指定 ref 的值为给定提交哈希。
 *
 * 会根据需要自动创建中间目录，例如 "refs/heads"。
 * ref 文件内容将被写为："<hash>\\n"。
 *
 * @param fs      仓库根目录对应的文件系统对象。
 * @param refname ref 路径，例如 "refs/heads/master"。
 * @param hash    目标提交哈希（40 位十六进制字符串）。
 * @return 写入成功返回 true，否则返回 false。
 */
bool update_ref(const FileSystem& fs,
                const std::string& refname,
                const std::string& hash);

/**
 * @brief 读取指定 ref 的当前提交哈希。
 *
 * 若 ref 文件不存在或内容为空，则返回 false。
 * 会自动去除末尾换行符。
 *
 * @param fs      仓库根目录对应的文件系统对象。
 * @param refname ref 路径，例如 "refs/heads/master"。
 * @param hash    输出参数，用于接收读取到的提交哈希。
 * @return 读取成功且内容合法时返回 true，否则返回 false。
 */
bool read_ref(const FileSystem& fs,
              const std::string& refname,
              std::string& hash);

}  // namespace minigit

