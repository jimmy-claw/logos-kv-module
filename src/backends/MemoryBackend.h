#pragma once

#include "KvBackend.h"

#include <shared_mutex>
#include <unordered_map>

class MemoryBackend : public KvBackend {
public:
    void set(const std::string &key, const std::string &value) override;
    std::optional<std::string> get(const std::string &key) override;
    void remove(const std::string &key) override;
    std::vector<std::string> list(const std::string &prefix) override;
    void clear() override;

    std::vector<std::string> scan(const std::string &pattern) override;
    std::vector<std::pair<std::string, std::string>>
        searchValues(const std::string &substring) override;

private:
    std::unordered_map<std::string, std::string> store_;
    mutable std::shared_mutex mutex_;
};
