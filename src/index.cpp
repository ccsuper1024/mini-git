#include "index.h"

#include <cstddef>
#include <sstream>

// 本文件实现 index（暂存区）文件的解析与写回逻辑
namespace {

// 去除字符串末尾的 '\n' 或 '\r\n'
std::string rstrip_newline(const std::string& s) {
    if (s.empty()) {
        return s;
    }
    std::size_t end = s.size();
    while (end > 0 && (s[end - 1] == '\n' || s[end - 1] == '\r')) {
        --end;
    }
    if (end == s.size()) {
        return s;
    }
    return s.substr(0, end);
}

}  // namespace

namespace minigit {

bool read_index(const FileSystem& fs, std::vector<IndexEntry>& entries) {
    entries.clear();

    std::string data;
    if (!fs.read_file("index", data)) {
        return true;
    }

    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        line = rstrip_newline(line);
        if (line.empty()) {
            continue;
        }

        std::size_t first_space = line.find(' ');
        if (first_space == std::string::npos) {
            return false;
        }
        std::size_t second_space = line.find(' ', first_space + 1);
        if (second_space == std::string::npos) {
            return false;
        }

        IndexEntry e;
        e.mode = line.substr(0, first_space);
        e.hash = line.substr(first_space + 1, second_space - first_space - 1);
        e.path = line.substr(second_space + 1);
        if (e.mode.empty() || e.hash.size() != 40U || e.path.empty()) {
            return false;
        }

        entries.push_back(e);
    }

    return true;
}

bool write_index(const FileSystem& fs, const std::vector<IndexEntry>& entries) {
    std::string data;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const IndexEntry& e = entries[i];
        data.append(e.mode);
        data.push_back(' ');
        data.append(e.hash);
        data.push_back(' ');
        data.append(e.path);
        data.push_back('\n');
    }
    return fs.write_file("index", data);
}

void upsert_index_entry(std::vector<IndexEntry>& entries,
                        const IndexEntry& entry) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].path == entry.path) {
            entries[i].mode = entry.mode;
            entries[i].hash = entry.hash;
            return;
        }
    }
    entries.push_back(entry);
}

}  // namespace minigit

