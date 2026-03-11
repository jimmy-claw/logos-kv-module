#pragma once

#include "KvBackend.h"

#include <filesystem>
#include <memory>
#include <mutex>

namespace rocksdb {
class DB;
}

class RocksDbBackend : public KvBackend {
public:
    explicit RocksDbBackend(std::filesystem::path db_path);
    ~RocksDbBackend() override;

    void set(const std::string &key, const std::string &value) override;
    std::optional<std::string> get(const std::string &key) override;
    void remove(const std::string &key) override;
    std::vector<std::string> list(const std::string &prefix) override;
    void clear() override;

    std::vector<std::string> scan(const std::string &pattern) override;
    std::vector<std::pair<std::string, std::string>>
        searchValues(const std::string &substring) override;

private:
    std::filesystem::path db_path_;
    rocksdb::DB *db_ = nullptr;
    std::mutex write_mutex_;
};
