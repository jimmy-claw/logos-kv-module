#include "FileBackend.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

// Minimal JSON helpers — no external dependency required.
// The on-disk format is a flat JSON object: {"key": "value", ...}

namespace {

std::string escapeJson(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

std::string unescapeJson(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case '"':  out += '"';  ++i; break;
            case '\\': out += '\\'; ++i; break;
            case 'n':  out += '\n'; ++i; break;
            case 'r':  out += '\r'; ++i; break;
            case 't':  out += '\t'; ++i; break;
            default:   out += s[i];      break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Parse a quoted JSON string starting at pos (which points at the opening '"').
// Advances pos past the closing '"'.
std::string parseJsonString(const std::string &json, size_t &pos) {
    if (pos >= json.size() || json[pos] != '"')
        return {};
    ++pos; // skip opening quote
    std::string raw;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            raw += json[pos];
            raw += json[pos + 1];
            pos += 2;
        } else {
            raw += json[pos];
            ++pos;
        }
    }
    if (pos < json.size())
        ++pos; // skip closing quote
    return unescapeJson(raw);
}

void skipWhitespace(const std::string &json, size_t &pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
        ++pos;
}

std::unordered_map<std::string, std::string> parseJson(const std::string &json) {
    std::unordered_map<std::string, std::string> map;
    size_t pos = 0;
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '{')
        return map;
    ++pos; // skip '{'

    while (pos < json.size()) {
        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] == '}')
            break;
        if (json[pos] == ',') { ++pos; continue; }

        std::string key = parseJsonString(json, pos);
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ':')
            ++pos;
        skipWhitespace(json, pos);
        std::string value = parseJsonString(json, pos);
        map[key] = value;
    }
    return map;
}

std::string toJson(const std::unordered_map<std::string, std::string> &map) {
    std::ostringstream out;
    out << "{\n";
    size_t i = 0;
    for (const auto &[k, v] : map) {
        out << "  \"" << escapeJson(k) << "\": \"" << escapeJson(v) << "\"";
        if (++i < map.size())
            out << ",";
        out << "\n";
    }
    out << "}\n";
    return out.str();
}

bool containsCaseInsensitive(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

} // anonymous namespace

FileBackend::FileBackend(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
    std::filesystem::create_directories(data_dir_);
}

std::filesystem::path FileBackend::filePath() const {
    return data_dir_ / "store.json";
}

std::unordered_map<std::string, std::string> FileBackend::load() {
    auto path = filePath();
    if (!std::filesystem::exists(path))
        return {};
    std::ifstream in(path);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseJson(ss.str());
}

void FileBackend::save(const std::unordered_map<std::string, std::string> &data) {
    auto path = filePath();
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp);
        out << toJson(data);
    }
    std::filesystem::rename(tmp, path);
}

void FileBackend::set(const std::string &key, const std::string &value) {
    std::lock_guard lock(mutex_);
    auto data = load();
    data[key] = value;
    save(data);
}

std::optional<std::string> FileBackend::get(const std::string &key) {
    std::lock_guard lock(mutex_);
    auto data = load();
    auto it = data.find(key);
    if (it == data.end())
        return std::nullopt;
    return it->second;
}

void FileBackend::remove(const std::string &key) {
    std::lock_guard lock(mutex_);
    auto data = load();
    data.erase(key);
    save(data);
}

std::vector<std::string> FileBackend::list(const std::string &prefix) {
    std::lock_guard lock(mutex_);
    auto data = load();
    std::vector<std::string> result;
    for (const auto &[k, v] : data) {
        if (prefix.empty() || k.compare(0, prefix.size(), prefix) == 0)
            result.push_back(k);
    }
    return result;
}

void FileBackend::clear() {
    std::lock_guard lock(mutex_);
    save({});
}

std::vector<std::string> FileBackend::scan(const std::string &pattern) {
    std::lock_guard lock(mutex_);
    auto data = load();
    std::vector<std::string> result;
    for (const auto &[k, v] : data) {
        if (pattern.empty() || k.find(pattern) != std::string::npos)
            result.push_back(k);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>>
FileBackend::searchValues(const std::string &substring) {
    std::lock_guard lock(mutex_);
    auto data = load();
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &[k, v] : data) {
        if (containsCaseInsensitive(v, substring))
            result.emplace_back(k, v);
    }
    return result;
}
