#pragma once

#include <map>
#include <string>

#include "filesystem.h"

namespace minigit {

struct PackedEntry {
    std::string hash;
    std::string compressed;
};

bool write_pack_file(FileSystem& fs, const std::string& pack_relative_path);

bool read_pack_file(FileSystem& fs,
                    const std::string& pack_relative_path,
                    std::map<std::string, PackedEntry>& out_entries);

}  // namespace minigit

