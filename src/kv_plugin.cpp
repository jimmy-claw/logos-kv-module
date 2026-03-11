#include "kv_plugin.h"
#include "backends/MemoryBackend.h"
#include "backends/FileBackend.h"

#ifdef LOGOS_CORE_AVAILABLE
#include <logos_api_provider.h>
#include <logos_api_client.h>
#endif

#include <QDebug>
#include <filesystem>

#include <openssl/evp.h>
#include <openssl/rand.h>

KvPlugin::KvPlugin(QObject *parent)
    : QObject(parent) {}

// ── Logos Core lifecycle ─────────────────────────────────────────────────────

#ifdef LOGOS_CORE_AVAILABLE
void KvPlugin::initLogos(LogosAPI* logosAPIInstance) {
    logosAPI = logosAPIInstance;

    if (!logosAPI) {
        qWarning() << "KvPlugin: initLogos called with null LogosAPI";
        qInfo() << "KvPlugin: initialized (headless). version:" << version();
        return;
    }

    // NOTE: Do NOT call logosAPI->getProvider()->registerObject() here.
    // In logos_host subprocess mode, the SDK wraps us in a ModuleProxy
    // that handles registration automatically. Calling registerObject()
    // directly causes a segfault in the QHash destructor.

    m_client = logosAPI->getClient(name());
    if (!m_client) {
        qWarning() << "KvPlugin: failed to get client handle for" << name();
    }

    const QString dirProp = logosAPI->property("kvDataDir").toString();
    if (!dirProp.isEmpty())
        setDataDir(dirProp);

    qInfo() << "KvPlugin: initialized. version:" << version();
}
#endif

// ── Configuration ────────────────────────────────────────────────────────────

void KvPlugin::setDataDir(const QString &path) {
    data_dir_ = path;
    use_file_backend_ = !path.isEmpty();
}

// ── Backend management ───────────────────────────────────────────────────────

KvBackend &KvPlugin::backendForNamespace(const std::string &ns) {
    std::lock_guard lock(backends_mutex_);
    auto it = backends_.find(ns);
    if (it != backends_.end())
        return *it->second;

    std::unique_ptr<KvBackend> backend;
    if (use_file_backend_) {
        auto dir = std::filesystem::path(data_dir_.toStdString()) / ns;
        backend = std::make_unique<FileBackend>(dir);
    } else {
        backend = std::make_unique<MemoryBackend>();
    }

    auto &ref = *backend;
    backends_[ns] = std::move(backend);
    return ref;
}

// ── Encryption ───────────────────────────────────────────────────────────────

void KvPlugin::setEncryptionKey(const QString& ns, const QString& keyHex) {
    QByteArray key = QByteArray::fromHex(keyHex.toLatin1());
    if (key.size() != 32) {
        qWarning() << "KvPlugin::setEncryptionKey: key must be 64 hex chars (32 bytes), got"
                   << keyHex.size() << "hex chars";
        return;
    }
    encryption_keys_[ns] = key;
}

QByteArray KvPlugin::encrypt(const QByteArray& key, const QByteArray& plaintext) const {
    constexpr int NONCE_LEN = 12;
    constexpr int TAG_LEN = 16;

    QByteArray nonce(NONCE_LEN, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), NONCE_LEN);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray ciphertext(plaintext.size(), '\0');
    QByteArray tag(TAG_LEN, '\0');
    int outLen = 0;

    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
           && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                  reinterpret_cast<const unsigned char*>(key.constData()),
                  reinterpret_cast<const unsigned char*>(nonce.constData()))
           && EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()),
                  &outLen, reinterpret_cast<const unsigned char*>(plaintext.constData()),
                  plaintext.size())
           && EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + outLen, &outLen)
           && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN,
                  reinterpret_cast<unsigned char*>(tag.data()));

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};

    // Format: nonce(12) + ciphertext + tag(16), then base64
    QByteArray result = nonce + ciphertext + tag;
    return result.toBase64();
}

QByteArray KvPlugin::decrypt(const QByteArray& key, const QByteArray& ciphertext) const {
    constexpr int NONCE_LEN = 12;
    constexpr int TAG_LEN = 16;

    QByteArray raw = QByteArray::fromBase64(ciphertext);
    if (raw.size() < NONCE_LEN + TAG_LEN) return {};

    QByteArray nonce = raw.left(NONCE_LEN);
    QByteArray tag = raw.right(TAG_LEN);
    QByteArray encrypted = raw.mid(NONCE_LEN, raw.size() - NONCE_LEN - TAG_LEN);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray plaintext(encrypted.size(), '\0');
    int outLen = 0;

    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
           && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                  reinterpret_cast<const unsigned char*>(key.constData()),
                  reinterpret_cast<const unsigned char*>(nonce.constData()))
           && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()),
                  &outLen, reinterpret_cast<const unsigned char*>(encrypted.constData()),
                  encrypted.size())
           && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN,
                  const_cast<char*>(tag.constData()));

    int finalLen = 0;
    ok = ok && EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + outLen, &finalLen);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};

    return plaintext;
}

// ── IKvModule operations ─────────────────────────────────────────────────────

void KvPlugin::set(const QString& ns, const QString& key, const QString& value) {
    QString storeValue = value;
    auto it = encryption_keys_.find(ns);
    if (it != encryption_keys_.end()) {
        QByteArray encrypted = encrypt(it.value(), value.toUtf8());
        if (encrypted.isEmpty()) {
            qWarning() << "KvPlugin::set: encryption failed for" << ns << key;
            return;
        }
        storeValue = QString::fromLatin1(encrypted);
    }
    backendForNamespace(ns.toStdString()).set(key.toStdString(), storeValue.toStdString());
    emit changed(ns, key);
}

QString KvPlugin::get(const QString& ns, const QString& key) {
    auto result = backendForNamespace(ns.toStdString()).get(key.toStdString());
    if (!result)
        return {};
    auto it = encryption_keys_.find(ns);
    if (it != encryption_keys_.end()) {
        QByteArray decrypted = decrypt(it.value(), QByteArray::fromStdString(*result));
        if (decrypted.isEmpty())
            return {};
        return QString::fromUtf8(decrypted);
    }
    return QString::fromStdString(*result);
}

void KvPlugin::remove(const QString& ns, const QString& key) {
    backendForNamespace(ns.toStdString()).remove(key.toStdString());
    emit changed(ns, key);
}

QString KvPlugin::list(const QString& ns, const QString& prefix) {
    auto keys = backendForNamespace(ns.toStdString()).list(prefix.toStdString());
    QString result = QStringLiteral("[");
    bool first = true;
    for (const auto &k : keys) {
        if (!first) result += ',';
        result += '"' + QString::fromStdString(k) + '"';
        first = false;
    }
    result += ']';
    return result;
}

QString KvPlugin::listAll(const QString& ns) {
    return list(ns, QString());
}

void KvPlugin::clear(const QString& ns) {
    backendForNamespace(ns.toStdString()).clear();
    emit changed(ns, QString());
}

QString KvPlugin::scan(const QString& ns, const QString& pattern) {
    auto keys = backendForNamespace(ns.toStdString()).scan(pattern.toStdString());
    QString result = QStringLiteral("[");
    bool first = true;
    for (const auto &k : keys) {
        if (!first) result += ',';
        result += '"' + QString::fromStdString(k) + '"';
        first = false;
    }
    result += ']';
    return result;
}

QString KvPlugin::searchValues(const QString& ns, const QString& substring) {
    auto encIt = encryption_keys_.find(ns);
    bool encrypted = (encIt != encryption_keys_.end());

    // For encrypted namespaces, get all entries and decrypt before matching
    auto entries = encrypted
        ? backendForNamespace(ns.toStdString()).searchValues(std::string())
        : backendForNamespace(ns.toStdString()).searchValues(substring.toStdString());

    QString result = QStringLiteral("[");
    bool first = true;
    for (const auto &[k, v] : entries) {
        QString value;
        if (encrypted) {
            QByteArray decrypted = decrypt(encIt.value(), QByteArray::fromStdString(v));
            if (decrypted.isEmpty())
                continue;
            value = QString::fromUtf8(decrypted);
            if (!value.contains(substring, Qt::CaseInsensitive))
                continue;
        } else {
            value = QString::fromStdString(v);
        }
        if (!first) result += ',';
        // Escape quotes in key and value for JSON safety
        QString escapedKey = QString::fromStdString(k);
        escapedKey.replace('\\', QStringLiteral("\\\\")).replace('"', QStringLiteral("\\\""));
        value.replace('\\', QStringLiteral("\\\\")).replace('"', QStringLiteral("\\\""));
        result += QStringLiteral("{\"key\":\"") + escapedKey
                + QStringLiteral("\",\"value\":\"") + value
                + QStringLiteral("\"}");
        first = false;
    }
    result += ']';
    return result;
}
