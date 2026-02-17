#include "commit.h"

#include <cstddef>
#include <string>

// 本文件实现 commit 对象的编码、解码及写入逻辑
namespace {

// 从给定位置读取一行（包含到换行符之前），返回行内容并更新索引
std::string read_line(const std::string& s, std::size_t& index) {
    if (index >= s.size()) {
        return std::string();
    }
    std::size_t pos = s.find('\n', index);
    if (pos == std::string::npos) {
        std::string line = s.substr(index);
        index = s.size();
        return line;
    }
    std::string line = s.substr(index, pos - index);
    index = pos + 1;
    return line;
}

}  // namespace

namespace minigit {

std::string build_commit_object(const Commit& commit) {
    std::string body;

    body.append("tree ");
    body.append(commit.tree);
    body.push_back('\n');

    for (std::size_t i = 0; i < commit.parents.size(); ++i) {
        body.append("parent ");
        body.append(commit.parents[i]);
        body.push_back('\n');
    }

    body.append("author ");
    body.append(commit.author);
    body.push_back('\n');

    body.append("committer ");
    body.append(commit.committer);
    body.push_back('\n');

    body.push_back('\n');
    body.append(commit.message);

    std::string header = "commit " + std::to_string(body.size());
    header.push_back('\0');

    std::string content;
    content.reserve(header.size() + body.size());
    content.append(header);
    content.append(body);
    return content;
}

bool parse_commit_object(const std::string& content, Commit& commit) {
    commit = Commit();

    std::size_t idx = 0;

    // 兼容两种输入：带头部的完整对象 或 仅正文
    std::size_t pos = content.find('\0');
    if (pos != std::string::npos) {
        std::string header = content.substr(0, pos);
        if (header.compare(0, 7, "commit ") == 0) {
            idx = pos + 1;
        } else {
            idx = 0;
        }
    }

    // 逐行解析头部字段，直到遇到空行
    while (idx < content.size()) {
        std::size_t line_start = idx;
        std::string line = read_line(content, idx);

        if (line.empty()) {
            // 空行：后续为提交消息
            break;
        }

        if (line.compare(0, 5, "tree ") == 0) {
            commit.tree = line.substr(5);
        } else if (line.compare(0, 7, "parent ") == 0) {
            commit.parents.push_back(line.substr(7));
        } else if (line.compare(0, 7, "author ") == 0) {
            commit.author = line.substr(7);
        } else if (line.compare(0, 10, "committer ") == 0) {
            commit.committer = line.substr(10);
        } else {
            // 未识别的头字段，保持兼容性：忽略该行
            (void)line_start;
        }
    }

    if (idx < content.size()) {
        // 剩余部分全部作为提交消息
        commit.message = content.substr(idx);
    }

    // 最基本的有效性检查：必须至少包含 tree
    if (commit.tree.empty()) {
        return false;
    }
    return true;
}

std::string write_commit(ObjectStore& store, const Commit& commit) {
    std::string content = build_commit_object(commit);
    return store.store_commit(content);
}

}  // namespace minigit

