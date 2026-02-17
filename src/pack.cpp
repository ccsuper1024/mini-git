#include "pack.h"

#include <cstdint>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

namespace minigit {

static void write_u32_be(std::string& out, std::uint32_t v) {
    out.push_back(static_cast<char>((v >> 24) & 0xff));
    out.push_back(static_cast<char>((v >> 16) & 0xff));
    out.push_back(static_cast<char>((v >> 8) & 0xff));
    out.push_back(static_cast<char>(v & 0xff));
}

static bool read_u32_be(const std::string& data, std::size_t& offset, std::uint32_t& v) {
    if (offset + 4 > data.size()) {
        return false;
    }
    unsigned char b0 = static_cast<unsigned char>(data[offset]);
    unsigned char b1 = static_cast<unsigned char>(data[offset + 1]);
    unsigned char b2 = static_cast<unsigned char>(data[offset + 2]);
    unsigned char b3 = static_cast<unsigned char>(data[offset + 3]);
    offset += 4;
    v = (static_cast<std::uint32_t>(b0) << 24) |
        (static_cast<std::uint32_t>(b1) << 16) |
        (static_cast<std::uint32_t>(b2) << 8) |
        static_cast<std::uint32_t>(b3);
    return true;
}

static void scan_objects_dir(FileSystem& fs, const std::string& objects_dir,
                             std::vector<PackedEntry>& out_entries) {
    std::string root = fs.make_path(objects_dir);
    DIR* d = opendir(root.c_str());
    if (!d) {
        return;
    }
    dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (std::string(de->d_name) == "." || std::string(de->d_name) == "..") {
            continue;
        }
        std::string subdir = root + "/" + de->d_name;
        struct stat st;
        if (stat(subdir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }
        std::string prefix = de->d_name;
        DIR* d2 = opendir(subdir.c_str());
        if (!d2) {
            continue;
        }
        dirent* de2;
        while ((de2 = readdir(d2)) != nullptr) {
            if (std::string(de2->d_name) == "." || std::string(de2->d_name) == "..") {
                continue;
            }
            std::string fname = de2->d_name;
            std::string rel = objects_dir + "/" + prefix + "/" + fname;
            std::string compressed;
            if (!fs.read_file(rel, compressed)) {
                continue;
            }
            PackedEntry e;
            e.hash = prefix + fname;
            e.compressed = compressed;
            out_entries.push_back(e);
        }
        closedir(d2);
    }
    closedir(d);
}

bool write_pack_file(FileSystem& fs, const std::string& pack_relative_path) {
    std::vector<PackedEntry> entries;
    scan_objects_dir(fs, "objects", entries);
    if (entries.empty()) {
        return false;
    }
    std::string pack;
    pack.append("MPK1", 4);
    write_u32_be(pack, static_cast<std::uint32_t>(entries.size()));
    for (const auto& e : entries) {
        if (e.hash.size() != 40) {
            continue;
        }
        pack.append(e.hash);
        write_u32_be(pack, static_cast<std::uint32_t>(e.compressed.size()));
        pack.append(e.compressed);
    }
    std::size_t pos = pack_relative_path.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = pack_relative_path.substr(0, pos);
        fs.ensure_directory(dir);
    }
    return fs.write_file(pack_relative_path, pack);
}

bool read_pack_file(FileSystem& fs,
                    const std::string& pack_relative_path,
                    std::map<std::string, PackedEntry>& out_entries) {
    std::string data;
    if (!fs.read_file(pack_relative_path, data)) {
        return false;
    }
    if (data.size() < 8) {
        return false;
    }
    if (data.compare(0, 4, "MPK1") != 0) {
        return false;
    }
    std::size_t offset = 4;
    std::uint32_t count = 0;
    if (!read_u32_be(data, offset, count)) {
        return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        if (offset + 40 + 4 > data.size()) {
            return false;
        }
        std::string hash = data.substr(offset, 40);
        offset += 40;
        std::uint32_t sz = 0;
        if (!read_u32_be(data, offset, sz)) {
            return false;
        }
        if (offset + sz > data.size()) {
            return false;
        }
        std::string compressed = data.substr(offset, sz);
        offset += sz;
        PackedEntry e;
        e.hash = hash;
        e.compressed = compressed;
        out_entries[hash] = e;
    }
    return true;
}

}  // namespace minigit

