#pragma once

#include "i_kv_module.h"
#include "backends/KvBackend.h"

#include <QByteArray>
#include <QMap>

#include <memory>
#include <mutex>
#include <unordered_map>

#ifdef LOGOS_CORE_AVAILABLE
#include <interface.h>
class LogosAPIClient;

class KvPlugin final : public QObject, public PluginInterface, public IKvModule {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IKvModule_iid FILE "metadata.json")
    Q_INTERFACES(PluginInterface IKvModule)
#else
class KvPlugin final : public QObject, public IKvModule {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IKvModule_iid FILE "metadata.json")
    Q_INTERFACES(IKvModule)
#endif

public:
    explicit KvPlugin(QObject *parent = nullptr);
    ~KvPlugin() override = default;

    // ── PluginInterface ─────────────────────────────────────────────────────
#ifdef LOGOS_CORE_AVAILABLE
    [[nodiscard]] QString name() const override { return QStringLiteral("kv_module"); }
    Q_INVOKABLE QString version() const override { return QStringLiteral("0.1.0"); }
    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);
#else
    [[nodiscard]] QString name() const { return QStringLiteral("kv_module"); }
    Q_INVOKABLE QString version() const { return QStringLiteral("0.1.0"); }
#endif

    // IKvModule operations
    Q_INVOKABLE void set(const QString& ns, const QString& key, const QString& value) override;
    Q_INVOKABLE QString get(const QString& ns, const QString& key) override;
    Q_INVOKABLE void remove(const QString& ns, const QString& key) override;
    Q_INVOKABLE QString list(const QString& ns, const QString& prefix) override;
    Q_INVOKABLE QString listAll(const QString& ns) override;
    Q_INVOKABLE void clear(const QString& ns) override;

    void setDataDir(const QString &path);

    // ── Encryption ──────────────────────────────────────────────────────────
    Q_INVOKABLE void setEncryptionKey(const QString& ns, const QString& keyHex);

signals:
    void changed(const QString& ns, const QString& key);

private:
    KvBackend &backendForNamespace(const std::string &ns);

    QString data_dir_;
    std::mutex backends_mutex_;
    std::unordered_map<std::string, std::unique_ptr<KvBackend>> backends_;
    bool use_file_backend_ = false;

    // ── Encryption ──────────────────────────────────────────────────────────
    QMap<QString, QByteArray> encryption_keys_; // ns -> 32-byte key
    QByteArray encrypt(const QByteArray& key, const QByteArray& plaintext) const;
    QByteArray decrypt(const QByteArray& key, const QByteArray& ciphertext) const;

#ifdef LOGOS_CORE_AVAILABLE
    LogosAPIClient* m_client = nullptr;
#endif
};
