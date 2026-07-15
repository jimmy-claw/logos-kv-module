#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <logos_module_context.h>  // LogosModuleContext base + modules()

// Forward declarations for internal components
class KvBackend;

/**
 * KvImpl — Universal-pattern core module for key-value storage.
 *
 * Pure C++ class — no Qt, no Q_OBJECT, no Q_PLUGIN_METADATA.
 * All public methods are auto-exposed by logos-cpp-generator.
 * Supports swappable backends: memory (default), file-based.
 * Optional backends (rocksdb, sqlite) compiled via feature flags.
 */
class KvImpl : public LogosModuleContext {
public:
    KvImpl();
    ~KvImpl() override;

    // ── Configuration ────────────────────────────────────────────────────────
    void setDataDir(const std::string& path);
    std::string getDataDir() const;

    // ── KV Operations ────────────────────────────────────────────────────────
    void set(const std::string& ns, const std::string& key, const std::string& value);
    std::string get(const std::string& ns, const std::string& key) const;
    void remove(const std::string& ns, const std::string& key);
    std::string list(const std::string& ns, const std::string& prefix) const;  // JSON array
    std::string listAll(const std::string& ns) const;                          // JSON array
    void clear(const std::string& ns);

    // ── Encryption ───────────────────────────────────────────────────────────
    void setEncryptionKey(const std::string& ns, const std::string& keyHex);

private:
    KvBackend &backendForNamespace(const std::string &ns);

    std::string data_dir_;
    mutable std::mutex backends_mutex_;
    std::unordered_map<std::string, std::unique_ptr<KvBackend>> backends_;
    bool use_file_backend_ = false;

    // ── Encryption ───────────────────────────────────────────────────────────
    std::unordered_map<std::string, std::vector<uint8_t>> encryption_keys_; // ns -> 32-byte key
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& plaintext) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& ciphertext) const;
};
