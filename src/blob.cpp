#include "blob.h"

#include <sstream>
#include <string>

// 本文件实现 Git 风格的 blob 对象内容构造
namespace minigit {

// 使用 "blob <size>\\0<data>" 形式构造 blob 对象内容
std::string build_blob_object(const std::string& data) {
    std::ostringstream oss;
    oss << "blob " << data.size() << '\0';
    oss << data;
    return oss.str();
}

}  // namespace minigit
