#pragma once

#include <string>

// 本文件声明简单的文件系统抽象，用于在仓库根目录下读写文件
namespace minigit {

/**
 * @brief 仓库根目录下的文件系统辅助类。
 *
 * 封装常见的路径拼接、目录创建以及二进制文件读写操作，
 * 统一通过给定的根目录进行访问，避免在调用方重复处理路径。
 */
class FileSystem {
public:
    /**
     * @brief 使用给定根目录构造文件系统对象。
     *
     * @param root 仓库根目录路径，例如 ".minigit"。
     */
    explicit FileSystem(std::string root);

    /**
     * @brief 获取仓库根目录路径。
     *
     * @return 根目录的字符串引用。
     */
    const std::string& root() const;

    /**
     * @brief 将相对路径转换为基于根目录的路径。
     *
     * 不保证路径对应的文件或目录一定存在，只进行字符串拼接。
     *
     * @param relative 相对于根目录的路径。
     * @return 拼接后的完整路径字符串。
     */
    std::string make_path(const std::string& relative) const;

    /**
     * @brief 确保相对路径对应的目录存在。
     *
     * 如有需要会递归创建多级目录。
     *
     * @param relative 相对于根目录的目录路径。
     * @return 目录存在且为目录时返回 true，否则返回 false。
     */
    bool ensure_directory(const std::string& relative) const;

    /**
     * @brief 在给定相对路径写入二进制数据。
     *
     * 会根据需要创建上级目录，写入失败时返回 false。
     *
     * @param relative 相对于根目录的文件路径。
     * @param data     要写入的二进制数据。
     * @return 写入成功返回 true，否则返回 false。
     */
    bool write_file(const std::string& relative, const std::string& data) const;

    /**
     * @brief 从给定相对路径读取二进制数据。
     *
     * 若文件不存在或打开失败，返回 false 且不修改 out。
     *
     * @param relative 相对于根目录的文件路径。
     * @param out      输出参数，用于接收读取到的数据。
     * @return 读取成功返回 true，否则返回 false。
     */
    bool read_file(const std::string& relative, std::string& out) const;

    /**
     * @brief 判断给定相对路径是否存在。
     *
     * 可以是文件或目录，仅检查路径是否存在。
     *
     * @param relative 相对于根目录的路径。
     * @return 存在返回 true，否则返回 false。
     */
    bool exists(const std::string& relative) const;

private:
    std::string root_;
};

}  // namespace minigit
