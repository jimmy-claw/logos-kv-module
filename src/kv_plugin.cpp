#include "kv_plugin.h"
#include "backends/MemoryBackend.h"
#include "backends/FileBackend.h"

#include <filesystem>

KvPlugin::KvPlugin(QObject *parent)
    : KvInterface(parent) {}

void KvPlugin::setDataDir(const QString &path) {
    data_dir_ = path;
    use_file_backend_ = !path.isEmpty();
}

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

void KvPlugin::set(QString ns, QString key, QByteArray value) {
    auto nsStd = ns.toStdString();
    auto keyStd = key.toStdString();
    backendForNamespace(nsStd).set(keyStd, std::string(value.constData(), value.size()));
    emit changed(ns, key);
}

QByteArray KvPlugin::get(QString ns, QString key) {
    auto result = backendForNamespace(ns.toStdString()).get(key.toStdString());
    if (!result)
        return {};
    return QByteArray::fromStdString(*result);
}

void KvPlugin::remove(QString ns, QString key) {
    backendForNamespace(ns.toStdString()).remove(key.toStdString());
    emit changed(ns, key);
}

QStringList KvPlugin::list(QString ns, QString prefix) {
    auto keys = backendForNamespace(ns.toStdString()).list(prefix.toStdString());
    QStringList result;
    result.reserve(static_cast<int>(keys.size()));
    for (const auto &k : keys)
        result.append(QString::fromStdString(k));
    return result;
}

void KvPlugin::clear(QString ns) {
    backendForNamespace(ns.toStdString()).clear();
    emit changed(ns, QString());
}
