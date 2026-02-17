#include "zlib_utils.h"

#include <stdexcept>
#include <vector>

#include <zlib.h>

// 本文件实现基于 zlib 的压缩与解压工具函数
namespace minigit {

// 使用 zlib 对输入数据进行压缩，压缩失败时抛出异常
std::string zlib_compress(const std::string& input) {
    if (input.empty()) {
        return std::string();
    }

    uLong source_len = static_cast<uLong>(input.size());
    uLong dest_len = compressBound(source_len);
    std::vector<unsigned char> buffer(dest_len);

    int ret = compress2(buffer.data(), &dest_len,
                        reinterpret_cast<const Bytef*>(input.data()),
                        source_len, Z_BEST_COMPRESSION);

    if (ret != Z_OK) {
        throw std::runtime_error("zlib_compress failed");
    }

    return std::string(reinterpret_cast<char*>(buffer.data()), dest_len);
}

// 使用 zlib 对输入数据进行解压，自动扩容缓冲区直至解压成功
std::string zlib_decompress(const std::string& input) {
    if (input.empty()) {
        return std::string();
    }

    uLong dest_len = static_cast<uLong>(input.size()) * 4U;
    std::vector<unsigned char> buffer(dest_len);

    int ret = Z_BUF_ERROR;
    do {
        ret = uncompress(buffer.data(), &dest_len,
                         reinterpret_cast<const Bytef*>(input.data()),
                         static_cast<uLong>(input.size()));
        if (ret == Z_BUF_ERROR) {
            dest_len *= 2U;
            buffer.resize(dest_len);
        }
    } while (ret == Z_BUF_ERROR);

    if (ret != Z_OK) {
        throw std::runtime_error("zlib_decompress failed");
    }

    return std::string(reinterpret_cast<char*>(buffer.data()), dest_len);
}

}  // namespace minigit
