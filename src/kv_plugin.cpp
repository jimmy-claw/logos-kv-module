#include "kv_plugin.h"
#include "backends/MemoryBackend.h"
#include "backends/FileBackend.h"

#include <filesystem>

KvPlugin::KvPlugin(QObject *parent)
    : QObject(parent) {}

// ── Logos Core lifecycle ─────────────────────────────────────────────────────

void KvPlugin::initLogos(LogosAPI* logosAPIInstance) {
    logosAPI = logosAPIInstance;

    if (logosAPI) {
        const QString dirProp = logosAPI->property("kvDataDir").toString();
        if (!dirProp.isEmpty())
            setDataDir(dirProp);
    }
}

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

// ── IKvModule operations ─────────────────────────────────────────────────────

void KvPlugin::kvSet(const QString& ns, const QString& key, const QByteArray& value) {
    auto nsStd = ns.toStdString();
    auto keyStd = key.toStdString();
    backendForNamespace(nsStd).set(keyStd, std::string(value.constData(), value.size()));
    emit kvChanged(ns, key);
}

QByteArray KvPlugin::kvGet(const QString& ns, const QString& key) {
    auto result = backendForNamespace(ns.toStdString()).get(key.toStdString());
    if (!result)
        return {};
    return QByteArray::fromStdString(*result);
}

void KvPlugin::kvRemove(const QString& ns, const QString& key) {
    backendForNamespace(ns.toStdString()).remove(key.toStdString());
    emit kvChanged(ns, key);
}

QStringList KvPlugin::kvList(const QString& ns, const QString& prefix) {
    auto keys = backendForNamespace(ns.toStdString()).list(prefix.toStdString());
    QStringList result;
    result.reserve(static_cast<int>(keys.size()));
    for (const auto &k : keys)
        result.append(QString::fromStdString(k));
    return result;
}

void KvPlugin::kvClear(const QString& ns) {
    backendForNamespace(ns.toStdString()).clear();
    emit kvChanged(ns, QString());
}
