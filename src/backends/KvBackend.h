#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

class KvBackend {
public:
    virtual ~KvBackend() = default;

    virtual void set(const std::string &key, const std::string &value) = 0;
    virtual std::optional<std::string> get(const std::string &key) = 0;
    virtual void remove(const std::string &key) = 0;
    virtual std::vector<std::string> list(const std::string &prefix) = 0;
    virtual void clear() = 0;

    virtual std::vector<std::string> scan(const std::string &pattern) = 0;
    virtual std::vector<std::pair<std::string, std::string>>
        searchValues(const std::string &substring) = 0;
};
