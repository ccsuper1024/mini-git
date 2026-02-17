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

#include <ctime>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <cstdlib>
#include <cstdio>
#include <regex>

namespace minigit {

static std::string default_user_name() {
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_gecos && pw->pw_gecos[0] != '\0') {
        std::string gecos(pw->pw_gecos);
        std::size_t pos = gecos.find(',');
        if (pos != std::string::npos) {
            gecos = gecos.substr(0, pos);
        }
        return gecos;
    }
    if (pw && pw->pw_name) {
        return std::string(pw->pw_name);
    }
    return "unknown";
}

static std::string default_user_email() {
    struct passwd* pw = getpwuid(getuid());
    const char* uname = (pw && pw->pw_name) ? pw->pw_name : "user";
    char host[256] = {0};
    if (gethostname(host, sizeof(host) - 1) != 0 || host[0] == '\0') {
        std::snprintf(host, sizeof(host), "localhost");
    }
    return std::string(uname) + "@" + std::string(host);
}

static std::string epoch_with_offset() {
    std::time_t now = std::time(nullptr);
    std::tm lt;
    localtime_r(&now, &lt);
#ifdef __GLIBC__
    long off = lt.tm_gmtoff;
#else
    long off = 0;
#endif
    char sign = off >= 0 ? '+' : '-';
    if (off < 0) off = -off;
    int hh = static_cast<int>(off / 3600);
    int mm = static_cast<int>((off % 3600) / 60);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%ld %c%02d%02d", static_cast<long>(now), sign, hh, mm);
    return std::string(buf);
}

static std::string env_or(const char* key, const std::string& defval) {
    const char* v = std::getenv(key);
    if (v && v[0] != '\0') return std::string(v);
    return defval;
}

static bool parse_offset_string(const std::string& s, long& seconds, std::string& normalized) {
    std::regex re(R"(^([+-])(\d{2}):?(\d{2})$)");
    std::smatch m;
    if (!std::regex_match(s, m, re)) return false;
    int hh = std::stoi(m[2].str());
    int mm = std::stoi(m[3].str());
    seconds = (hh * 3600 + mm * 60);
    if (m[1].str() == "-") seconds = -seconds;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%s%02d%02d", m[1].str().c_str(), hh, mm);
    normalized.assign(buf);
    return true;
}

static bool parse_epoch_with_offset(const std::string& s, std::string& out) {
    std::regex re(R"(^\s*(\d{9,12})\s+([+-]\d{2}:?\d{2})\s*$)");
    std::smatch m;
    if (!std::regex_match(s, m, re)) return false;
    long off = 0;
    std::string norm;
    if (!parse_offset_string(m[2].str(), off, norm)) return false;
    out = m[1].str() + " " + norm;
    return true;
}

static bool parse_rfc2822_like(const std::string& s, std::string& out) {
    std::regex re(R"(^\s*(... .. .. ..:..:.. ....)\s+([+-]\d{2}:?\d{2})\s*$)");
    std::smatch m;
    if (!std::regex_match(s, m, re)) return false;
    std::string dt = m[1].str();
    std::string offstr = m[2].str();
    std::tm tm{};
    char* p = strptime(dt.c_str(), "%a %b %d %H:%M:%S %Y", &tm);
    if (!p) return false;
#ifdef _GNU_SOURCE
    time_t utc = timegm(&tm);
#else
    time_t utc = timegm(&tm);
#endif
    long off = 0;
    std::string norm;
    if (!parse_offset_string(offstr, off, norm)) return false;
    long epoch = static_cast<long>(utc) - off;
    out = std::to_string(epoch) + " " + norm;
    return true;
}

static bool parse_iso8601(const std::string& s, std::string& out) {
    std::regex re(R"(^\s*(\d{4})-(\d{2})-(\d{2})[T\s](\d{2}):(\d{2})(?::(\d{2}))?(?:\s*([+-]\d{2}:?\d{2}))?\s*$)");
    std::smatch m;
    if (!std::regex_match(s, m, re)) return false;
    int Y = std::stoi(m[1].str());
    int Mo = std::stoi(m[2].str());
    int D = std::stoi(m[3].str());
    int H = std::stoi(m[4].str());
    int Mi = std::stoi(m[5].str());
    int S = m[6].matched ? std::stoi(m[6].str()) : 0;
    std::tm tm{};
    tm.tm_year = Y - 1900;
    tm.tm_mon = Mo - 1;
    tm.tm_mday = D;
    tm.tm_hour = H;
    tm.tm_min = Mi;
    tm.tm_sec = S;
#ifdef _GNU_SOURCE
    time_t utc = timegm(&tm);
#else
    time_t utc = timegm(&tm);
#endif
    long off = 0;
    std::string norm;
    if (m[7].matched) {
        if (!parse_offset_string(m[7].str(), off, norm)) return false;
    } else {
        // 无偏移则使用本地偏移
        std::string eo = epoch_with_offset();
        std::regex reo(R"(^(\d{9,12})\s+([+-]\d{4})$)");
        std::smatch mm;
        if (std::regex_match(eo, mm, reo)) {
            norm = mm[2].str();
        } else {
            norm = "+0000";
        }
    }
    long epoch = static_cast<long>(utc) - off;
    out = std::to_string(epoch) + " " + norm;
    return true;
}

static std::string normalize_date_string(const std::string& s) {
    std::string out;
    if (parse_epoch_with_offset(s, out)) return out;
    if (parse_rfc2822_like(s, out)) return out;
    if (parse_iso8601(s, out)) return out;
    return s;
}

std::string build_identity_from_env(const char* name_env, const char* email_env, const char* date_env) {
    std::string name = env_or(name_env, default_user_name());
    std::string email = env_or(email_env, default_user_email());
    const char* dv = std::getenv(date_env);
    std::string date = dv && dv[0] != '\0' ? normalize_date_string(std::string(dv))
                                           : epoch_with_offset();
    return name + " <" + email + "> " + date;
}

}  // namespace minigit
