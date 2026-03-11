#include "MemoryBackend.h"

#include <algorithm>
#include <cctype>
#include <mutex>

namespace {
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
} // namespace

void MemoryBackend::set(const std::string &key, const std::string &value) {
    std::unique_lock lock(mutex_);
    store_[key] = value;
}

std::optional<std::string> MemoryBackend::get(const std::string &key) {
    std::shared_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end())
        return std::nullopt;
    return it->second;
}

void MemoryBackend::remove(const std::string &key) {
    std::unique_lock lock(mutex_);
    store_.erase(key);
}

std::vector<std::string> MemoryBackend::list(const std::string &prefix) {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto &[k, v] : store_) {
        if (prefix.empty() || k.compare(0, prefix.size(), prefix) == 0)
            result.push_back(k);
    }
    return result;
}

void MemoryBackend::clear() {
    std::unique_lock lock(mutex_);
    store_.clear();
}

std::vector<std::string> MemoryBackend::scan(const std::string &pattern) {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    for (const auto &[k, v] : store_) {
        if (pattern.empty() || k.find(pattern) != std::string::npos)
            result.push_back(k);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>>
MemoryBackend::searchValues(const std::string &substring) {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &[k, v] : store_) {
        if (containsCaseInsensitive(v, substring))
            result.emplace_back(k, v);
    }
    return result;
}
