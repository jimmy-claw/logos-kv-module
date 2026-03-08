#pragma once

#include "i_kv_module.h"
#include "kv_interface.h"
#include "backends/KvBackend.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class KvPlugin final : public QObject, public PluginInterface, public IKvModule {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IKvModule_iid FILE "metadata.json")
    Q_INTERFACES(PluginInterface IKvModule)

public:
    explicit KvPlugin(QObject *parent = nullptr);
    ~KvPlugin() override = default;

    // ── PluginInterface ──────────────────────────────────────────────────────

    [[nodiscard]] QString name() const override { return QStringLiteral("kv_module"); }
    Q_INVOKABLE QString version() const override { return QStringLiteral("0.1.0"); }

    // ── Logos Core lifecycle ─────────────────────────────────────────────────

    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);

    // ── IKvModule operations (Q_INVOKABLE for RPC) ──────────────────────────

    Q_INVOKABLE void kvSet(const QString& ns, const QString& key, const QByteArray& value) override;
    Q_INVOKABLE QByteArray kvGet(const QString& ns, const QString& key) override;
    Q_INVOKABLE void kvRemove(const QString& ns, const QString& key) override;
    Q_INVOKABLE QStringList kvList(const QString& ns, const QString& prefix) override;
    Q_INVOKABLE void kvClear(const QString& ns) override;

    void setDataDir(const QString &path);

signals:
    void kvChanged(const QString& ns, const QString& key);

private:
    KvBackend &backendForNamespace(const std::string &ns);

    QString data_dir_;
    std::mutex backends_mutex_;
    std::unordered_map<std::string, std::unique_ptr<KvBackend>> backends_;
    bool use_file_backend_ = false;
};
