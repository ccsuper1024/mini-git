#pragma once

#include <string>

// 本文件声明基于 zlib 的压缩与解压工具函数
namespace minigit {

/**
 * @brief 使用 zlib 对输入数据进行压缩。
 *
 * 内部使用 zlib 的 compress2 接口，根据输入大小自动计算输出缓冲区。
 *
 * @param input 原始未压缩数据。
 * @return 压缩后的二进制数据；若输入为空则返回空字符串。
 * @throws std::runtime_error 当压缩失败时抛出异常。
 */
std::string zlib_compress(const std::string& input);

/**
 * @brief 使用 zlib 对输入数据进行解压。
 *
 * 通过多次尝试自动扩容输出缓冲区，直到 uncompress 成功。
 *
 * @param input 压缩后的二进制数据。
 * @return 解压得到的原始数据；若输入为空则返回空字符串。
 * @throws std::runtime_error 当解压失败时抛出异常。
 */
std::string zlib_decompress(const std::string& input);

}  // namespace minigit
