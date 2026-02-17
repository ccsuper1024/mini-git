#include "hash.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

// 本文件实现 SHA-1 哈希算法，用于对对象内容进行散列
namespace {

// 对 32 位无符号整数进行循环左移
inline uint32_t left_rotate(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32U - bits));
}

// 对单个 512 比特数据块执行一轮 SHA-1 变换
void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];

    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }

    for (int i = 16; i < 80; ++i) {
        w[i] = left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f;
        uint32_t k;

        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = left_rotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = left_rotate(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}  // namespace

namespace minigit {

// 计算任意长度输入数据的 SHA-1 哈希值
std::string sha1_hex(const std::string& data) {
    uint32_t state[5] = {
        0x67452301U,
        0xEFCDAB89U,
        0x98BADCFEU,
        0x10325476U,
        0xC3D2E1F0U,
    };

    uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8U;

    std::vector<uint8_t> buffer(data.begin(), data.end());
    buffer.push_back(0x80U);

    while ((buffer.size() % 64U) != 56U) {
        buffer.push_back(0x00U);
    }

    for (int i = 7; i >= 0; --i) {
        buffer.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFFU));
    }

    for (std::size_t i = 0; i < buffer.size(); i += 64) {
        sha1_transform(state, &buffer[i]);
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 5; ++i) {
        oss << std::setw(8) << state[i];
    }

    return oss.str();
}

}  // namespace minigit
