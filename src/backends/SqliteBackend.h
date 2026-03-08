#pragma once

#include "KvBackend.h"

#include <filesystem>
#include <mutex>

struct sqlite3;
struct sqlite3_stmt;

class SqliteBackend : public KvBackend {
public:
    explicit SqliteBackend(std::filesystem::path db_path);
    ~SqliteBackend() override;

    void set(const std::string &key, const std::string &value) override;
    std::optional<std::string> get(const std::string &key) override;
    void remove(const std::string &key) override;
    std::vector<std::string> list(const std::string &prefix) override;
    void clear() override;

private:
    std::filesystem::path db_path_;
    sqlite3 *db_ = nullptr;
    sqlite3_stmt *stmt_set_ = nullptr;
    sqlite3_stmt *stmt_get_ = nullptr;
    sqlite3_stmt *stmt_remove_ = nullptr;
    sqlite3_stmt *stmt_clear_ = nullptr;
    std::mutex mutex_;

    void finalize();
};
