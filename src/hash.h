#pragma once

#include <string>

// 本文件声明用于计算 SHA-1 哈希值的接口
namespace minigit {

/**
 * @brief 计算输入数据的 SHA-1 哈希值。
 *
 * 按照 Git 对象同样的算法输出 160 比特哈希的十六进制表示。
 *
 * @param data 输入的原始数据。
 * @return 返回长度为 40 的十六进制字符串。
 */
std::string sha1_hex(const std::string& data);

}  // namespace minigit
