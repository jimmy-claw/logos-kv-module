#include "kv_module_impl.h"
#include "backends/MemoryBackend.h"
#include "backends/FileBackend.h"

#include <filesystem>
#include <iostream>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/rand.h>

KvModuleImpl::KvModuleImpl() {}
KvModuleImpl::~KvModuleImpl() = default;

// ── Configuration ────────────────────────────────────────────────────────────

void KvModuleImpl::setDataDir(const std::string &path) {
    data_dir_ = path;
    use_file_backend_ = !path.empty();
}

std::string KvModuleImpl::getDataDir() const {
    return data_dir_;
}

// ── Backend management ───────────────────────────────────────────────────────

KvBackend &KvModuleImpl::backendForNamespace(const std::string &ns) const {
    std::lock_guard lock(backends_mutex_);
    auto it = backends_.find(ns);
    if (it != backends_.end())
        return *it->second;

    std::unique_ptr<KvBackend> backend;
    if (use_file_backend_) {
        auto dir = std::filesystem::path(data_dir_) / ns;
        backend = std::make_unique<FileBackend>(dir);
    } else {
        backend = std::make_unique<MemoryBackend>();
    }

    auto &ref = *backend;
    backends_[ns] = std::move(backend);
    return ref;
}

// ── Encryption ───────────────────────────────────────────────────────────────

void KvModuleImpl::setEncryptionKey(const std::string &ns, const std::string &keyHex) {
    std::vector<uint8_t> key;
    for (size_t i = 0; i + 1 < keyHex.size(); i += 2) {
        key.push_back(static_cast<uint8_t>(std::stoi(keyHex.substr(i, 2), nullptr, 16)));
    }
    if (key.size() != 32) {
        std::cerr << "KvModuleImpl::setEncryptionKey: key must be 64 hex chars (32 bytes), got "
                  << keyHex.size() << " hex chars" << std::endl;
        return;
    }
    encryption_keys_[ns] = key;
}

std::vector<uint8_t> KvModuleImpl::encrypt(const std::vector<uint8_t> &key,
                                      const std::vector<uint8_t> &plaintext) const {
    constexpr int NONCE_LEN = 12;
    constexpr int TAG_LEN = 16;

    std::vector<uint8_t> nonce(NONCE_LEN);
    RAND_bytes(nonce.data(), NONCE_LEN);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> ciphertext(plaintext.size());
    std::vector<uint8_t> tag(TAG_LEN);
    int outLen = 0;

    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
           && EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data())
           && EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen, plaintext.data(),
                                static_cast<int>(plaintext.size()))
           && EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen, &outLen)
           && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag.data());

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};

    // Format: nonce(12) + ciphertext + tag(16), then base64
    std::vector<uint8_t> result;
    result.reserve(NONCE_LEN + ciphertext.size() + TAG_LEN);
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    result.insert(result.end(), tag.begin(), tag.end());

    // Simple base64 encoding (inline to avoid Qt dependency)
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    b64.reserve(((result.size() + 2) / 3) * 4);
    for (size_t i = 0; i < result.size(); i += 3) {
        uint32_t triplet = 0;
        for (int j = 0; j < 3; j++)
            triplet = (triplet << 8) | (i + j < result.size() ? result[i + j] : 0);
        b64.push_back(table[(triplet >> 18) & 0x3F]);
        b64.push_back(table[(triplet >> 12) & 0x3F]);
        b64.push_back(i + 1 < result.size() ? table[(triplet >> 6) & 0x3F] : '=');
        b64.push_back(i + 2 < result.size() ? table[triplet & 0x3F] : '=');
    }
    return std::vector<uint8_t>(b64.begin(), b64.end());
}

std::vector<uint8_t> KvModuleImpl::decrypt(const std::vector<uint8_t> &key,
                                      const std::vector<uint8_t> &ciphertext) const {
    constexpr int NONCE_LEN = 12;
    constexpr int TAG_LEN = 16;

    // Base64 decode (inline)
    static const unsigned char table[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,62,0,0,0,63, 52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,
        0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13, 14,15,16,17,18,19,20,21,22,23,24,25,
        0,0,0,0,0,0,26,27,28,29,30,31,32,33,34,35, 36,37,38,39,40,41,42,43,44,45,
        46,47,48,49,50,51,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    std::vector<uint8_t> raw;
    raw.reserve((ciphertext.size() / 4) * 3);
    const uint8_t *data = ciphertext.data();
    size_t len = ciphertext.size();
    for (size_t i = 0; i < len; i += 4) {
        if (i + 3 >= len) break;
        uint32_t triplet = 0;
        for (int j = 0; j < 4; j++) {
            char c = static_cast<char>(data[i + j]);
            triplet = (triplet << 6) | (c == '=' ? 0 : table[static_cast<unsigned char>(c)]);
        }
        raw.push_back((triplet >> 16) & 0xFF);
        if (data[i + 2] != '=') raw.push_back((triplet >> 8) & 0xFF);
        if (data[i + 3] != '=') raw.push_back(triplet & 0xFF);
    }

    if (raw.size() < NONCE_LEN + TAG_LEN) return {};

    std::vector<uint8_t> nonce(raw.begin(), raw.begin() + NONCE_LEN);
    std::vector<uint8_t> tag(raw.end() - TAG_LEN, raw.end());
    std::vector<uint8_t> encrypted(raw.begin() + NONCE_LEN, raw.end() - TAG_LEN);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> plaintext(encrypted.size());
    int outLen = 0;

    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
           && EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data())
           && EVP_DecryptUpdate(ctx, plaintext.data(), &outLen, encrypted.data(),
                                static_cast<int>(encrypted.size()))
           && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, tag.data());

    int finalLen = 0;
    ok = ok && EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen, &finalLen);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};

    plaintext.resize(outLen + finalLen);
    return plaintext;
}

// ── KV Operations ────────────────────────────────────────────────────────────

void KvModuleImpl::set(const std::string &ns, const std::string &key, const std::string &value) {
    std::string storeValue = value;
    auto it = encryption_keys_.find(ns);
    if (it != encryption_keys_.end()) {
        auto encrypted = encrypt(it->second,
            std::vector<uint8_t>(value.begin(), value.end()));
        if (encrypted.empty()) {
            std::cerr << "KvModuleImpl::set: encryption failed for " << ns << "/" << key << std::endl;
            return;
        }
        storeValue = std::string(encrypted.begin(), encrypted.end());
    }
    backendForNamespace(ns).set(key, storeValue);
}

std::string KvModuleImpl::get(const std::string &ns, const std::string &key) const {
    auto result = backendForNamespace(ns).get(key);
    if (!result)
        return {};
    auto it = encryption_keys_.find(ns);
    if (it != encryption_keys_.end()) {
        auto decrypted = decrypt(it->second,
            std::vector<uint8_t>(result->begin(), result->end()));
        if (decrypted.empty())
            return {};
        return std::string(decrypted.begin(), decrypted.end());
    }
    return *result;
}

void KvModuleImpl::remove(const std::string &ns, const std::string &key) {
    backendForNamespace(ns).remove(key);
}

std::string KvModuleImpl::list(const std::string &ns, const std::string &prefix) const {
    auto keys = backendForNamespace(ns).list(prefix);
    std::string result = "[";
    bool first = true;
    for (const auto &k : keys) {
        if (!first) result += ',';
        result += '"';
        result += k;
        result += '"';
        first = false;
    }
    result += ']';
    return result;
}

std::string KvModuleImpl::listAll(const std::string &ns) const {
    return list(ns, "");
}

void KvModuleImpl::clear(const std::string &ns) {
    backendForNamespace(ns).clear();
}
