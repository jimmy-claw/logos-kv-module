#include "MemoryBackend.h"

#include <mutex>

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
