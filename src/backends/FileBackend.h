#pragma once

#include "KvBackend.h"

#include <filesystem>
#include <mutex>

class FileBackend : public KvBackend {
public:
    explicit FileBackend(std::filesystem::path data_dir);

    void set(const std::string &key, const std::string &value) override;
    std::optional<std::string> get(const std::string &key) override;
    void remove(const std::string &key) override;
    std::vector<std::string> list(const std::string &prefix) override;
    void clear() override;

private:
    std::filesystem::path data_dir_;
    std::mutex mutex_;

    std::filesystem::path filePath() const;
    std::unordered_map<std::string, std::string> load();
    void save(const std::unordered_map<std::string, std::string> &data);
};
