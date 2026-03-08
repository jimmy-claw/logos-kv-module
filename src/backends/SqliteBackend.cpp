#include "SqliteBackend.h"

#include <sqlite3.h>
#include <stdexcept>

SqliteBackend::SqliteBackend(std::filesystem::path db_path)
    : db_path_(std::move(db_path)) {
    int rc = sqlite3_open(db_path_.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open SQLite DB at " +
                                 db_path_.string() + ": " + err);
    }

    // Create table
    char *errmsg = nullptr;
    rc = sqlite3_exec(db_,
        "CREATE TABLE IF NOT EXISTS kv ("
        "key TEXT PRIMARY KEY, "
        "value BLOB NOT NULL"
        ");",
        nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to create kv table: " + err);
    }

    // Prepare statements
    auto prepare = [&](const char *sql, sqlite3_stmt **stmt) {
        rc = sqlite3_prepare_v2(db_, sql, -1, stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::string err = sqlite3_errmsg(db_);
            finalize();
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error(
                std::string("Failed to prepare statement: ") + err);
        }
    };

    prepare("INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?)", &stmt_set_);
    prepare("SELECT value FROM kv WHERE key = ?", &stmt_get_);
    prepare("DELETE FROM kv WHERE key = ?", &stmt_remove_);
    prepare("DELETE FROM kv", &stmt_clear_);
}

SqliteBackend::~SqliteBackend() {
    finalize();
    if (db_) {
        sqlite3_close(db_);
    }
}

void SqliteBackend::finalize() {
    auto fin = [](sqlite3_stmt *&s) {
        if (s) {
            sqlite3_finalize(s);
            s = nullptr;
        }
    };
    fin(stmt_set_);
    fin(stmt_get_);
    fin(stmt_remove_);
    fin(stmt_clear_);
}

void SqliteBackend::set(const std::string &key, const std::string &value) {
    std::lock_guard lock(mutex_);
    sqlite3_reset(stmt_set_);
    sqlite3_bind_text(stmt_set_, 1, key.data(), static_cast<int>(key.size()),
                      SQLITE_STATIC);
    sqlite3_bind_blob(stmt_set_, 2, value.data(), static_cast<int>(value.size()),
                      SQLITE_STATIC);
    int rc = sqlite3_step(stmt_set_);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQLite set failed: ") +
                                 sqlite3_errmsg(db_));
    }
}

std::optional<std::string> SqliteBackend::get(const std::string &key) {
    std::lock_guard lock(mutex_);
    sqlite3_reset(stmt_get_);
    sqlite3_bind_text(stmt_get_, 1, key.data(), static_cast<int>(key.size()),
                      SQLITE_STATIC);
    int rc = sqlite3_step(stmt_get_);
    if (rc == SQLITE_ROW) {
        const void *blob = sqlite3_column_blob(stmt_get_, 0);
        int len = sqlite3_column_bytes(stmt_get_, 0);
        return std::string(static_cast<const char *>(blob),
                           static_cast<size_t>(len));
    }
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    throw std::runtime_error(std::string("SQLite get failed: ") +
                             sqlite3_errmsg(db_));
}

void SqliteBackend::remove(const std::string &key) {
    std::lock_guard lock(mutex_);
    sqlite3_reset(stmt_remove_);
    sqlite3_bind_text(stmt_remove_, 1, key.data(), static_cast<int>(key.size()),
                      SQLITE_STATIC);
    int rc = sqlite3_step(stmt_remove_);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQLite remove failed: ") +
                                 sqlite3_errmsg(db_));
    }
}

std::vector<std::string> SqliteBackend::list(const std::string &prefix) {
    std::lock_guard lock(mutex_);
    std::vector<std::string> result;

    sqlite3_stmt *stmt = nullptr;
    int rc;

    if (prefix.empty()) {
        rc = sqlite3_prepare_v2(db_, "SELECT key FROM kv ORDER BY key", -1,
                                &stmt, nullptr);
    } else {
        // Use range comparison to avoid LIKE escaping issues:
        // key >= prefix AND key < prefix_end
        // where prefix_end is prefix with last byte incremented.
        std::string upper = prefix;
        // Increment the last byte to form the exclusive upper bound.
        // Walk backwards to handle 0xFF overflow.
        bool found = false;
        for (auto it = upper.rbegin(); it != upper.rend(); ++it) {
            if (static_cast<unsigned char>(*it) < 0xFF) {
                ++(*it);
                // Erase everything after this position
                upper.erase(it.base(), upper.end());
                found = true;
                break;
            }
        }
        if (found) {
            rc = sqlite3_prepare_v2(
                db_,
                "SELECT key FROM kv WHERE key >= ? AND key < ? ORDER BY key",
                -1, &stmt, nullptr);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, prefix.data(),
                                  static_cast<int>(prefix.size()),
                                  SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, upper.data(),
                                  static_cast<int>(upper.size()),
                                  SQLITE_STATIC);
            }
        } else {
            // All bytes are 0xFF — match everything >= prefix
            rc = sqlite3_prepare_v2(
                db_, "SELECT key FROM kv WHERE key >= ? ORDER BY key", -1,
                &stmt, nullptr);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, prefix.data(),
                                  static_cast<int>(prefix.size()),
                                  SQLITE_STATIC);
            }
        }
    }

    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("SQLite list prepare failed: ") +
                                 sqlite3_errmsg(db_));
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *key =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        int len = sqlite3_column_bytes(stmt, 0);
        result.emplace_back(key, static_cast<size_t>(len));
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQLite list failed: ") +
                                 sqlite3_errmsg(db_));
    }

    return result;
}

void SqliteBackend::clear() {
    std::lock_guard lock(mutex_);
    sqlite3_reset(stmt_clear_);
    int rc = sqlite3_step(stmt_clear_);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQLite clear failed: ") +
                                 sqlite3_errmsg(db_));
    }
}
