#pragma once

#include <string>

// 本文件声明 blob 对象构造函数，用于生成 Git 风格 blob 对象内容
namespace minigit {

/**
 * @brief 根据原始数据构造 Git 风格的 blob 对象内容。
 *
 * 生成的二进制内容格式为："blob <size>\\0<data>"，与 Git 原生格式兼容。
 *
 * @param data 原始文件数据。
 * @return 包含对象头部和正文的完整 blob 对象内容。
 */
std::string build_blob_object(const std::string& data);

}  // namespace minigit
