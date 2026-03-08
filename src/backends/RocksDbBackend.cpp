#include "RocksDbBackend.h"

#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/write_batch.h>
#include <stdexcept>

RocksDbBackend::RocksDbBackend(std::filesystem::path db_path)
    : db_path_(std::move(db_path)) {
    rocksdb::Options options;
    options.create_if_missing = true;
    auto status = rocksdb::DB::Open(options, db_path_.string(), &db_);
    if (!status.ok()) {
        throw std::runtime_error("Failed to open RocksDB at " +
                                 db_path_.string() + ": " +
                                 status.ToString());
    }
}

RocksDbBackend::~RocksDbBackend() {
    delete db_;
}

void RocksDbBackend::set(const std::string &key, const std::string &value) {
    std::lock_guard lock(write_mutex_);
    auto status = db_->Put(rocksdb::WriteOptions(), key, value);
    if (!status.ok()) {
        throw std::runtime_error("RocksDB Put failed: " + status.ToString());
    }
}

std::optional<std::string> RocksDbBackend::get(const std::string &key) {
    std::string value;
    auto status = db_->Get(rocksdb::ReadOptions(), key, &value);
    if (status.IsNotFound())
        return std::nullopt;
    if (!status.ok()) {
        throw std::runtime_error("RocksDB Get failed: " + status.ToString());
    }
    return value;
}

void RocksDbBackend::remove(const std::string &key) {
    std::lock_guard lock(write_mutex_);
    auto status = db_->Delete(rocksdb::WriteOptions(), key);
    if (!status.ok()) {
        throw std::runtime_error("RocksDB Delete failed: " + status.ToString());
    }
}

std::vector<std::string> RocksDbBackend::list(const std::string &prefix) {
    std::vector<std::string> result;
    std::unique_ptr<rocksdb::Iterator> it(
        db_->NewIterator(rocksdb::ReadOptions()));

    if (prefix.empty()) {
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            result.emplace_back(it->key().data(), it->key().size());
        }
    } else {
        for (it->Seek(prefix); it->Valid(); it->Next()) {
            auto key = it->key();
            if (key.size() < prefix.size() ||
                key.ToString().compare(0, prefix.size(), prefix) != 0) {
                break;
            }
            result.emplace_back(key.data(), key.size());
        }
    }
    return result;
}

void RocksDbBackend::clear() {
    std::lock_guard lock(write_mutex_);
    rocksdb::WriteBatch batch;
    std::unique_ptr<rocksdb::Iterator> it(
        db_->NewIterator(rocksdb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        batch.Delete(it->key());
    }
    auto status = db_->Write(rocksdb::WriteOptions(), &batch);
    if (!status.ok()) {
        throw std::runtime_error("RocksDB clear failed: " + status.ToString());
    }
}
