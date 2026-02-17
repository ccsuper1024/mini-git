#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <spdlog/spdlog.h>

#include "object_store.h"
#include "tree.h"

// 本文件实现 mini-git 命令行入口及子命令分发
namespace {

// 实现 hash-object 子命令，将文件内容存储为 blob 对象
int command_hash_object(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: mini-git hash-object <file>\n";
        return 1;
    }

    std::string path = argv[2];
    std::ifstream ifs(path.c_str(), std::ios::binary);
    if (!ifs) {
        std::cerr << "failed to open file: " << path << "\n";
        return 1;
    }

    std::string data((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());

    minigit::ObjectStore store(".minigit");
    std::string hash = store.store_blob(data);

    spdlog::info("stored blob {}", hash);
    std::cout << hash << "\n";
    return 0;
}

// 实现 write-tree 子命令，从当前工作目录构建目录快照
int command_write_tree(int /*argc*/, char** /*argv*/) {
    minigit::ObjectStore store(".minigit");
    std::string tree_hash = minigit::write_tree(store, ".");
    spdlog::info("write tree {}", tree_hash);
    std::cout << tree_hash << "\n";
    return 0;
}

}  // namespace

// 程序入口，根据第一个参数选择执行的子命令
int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);

    if (argc < 2) {
        std::cerr << "usage: mini-git <command> [args]\n";
        std::cerr << "commands:\n";
        std::cerr << "  hash-object <file>\n";
        std::cerr << "  write-tree\n";
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "hash-object") {
        return command_hash_object(argc, argv);
    }
    if (cmd == "write-tree") {
        return command_write_tree(argc, argv);
    }

    std::cerr << "unknown command: " << cmd << "\n";
    return 1;
}
