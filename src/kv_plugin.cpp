#include "kv_plugin.h"
#include "backends/MemoryBackend.h"
#include "backends/FileBackend.h"

#ifdef LOGOS_CORE_AVAILABLE
#include <logos_api_provider.h>
#include <logos_api_client.h>
#endif

#include <QDebug>
#include <filesystem>

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

// ── IKvModule operations ─────────────────────────────────────────────────────

void KvPlugin::set(const QString& ns, const QString& key, const QString& value) {
    backendForNamespace(ns.toStdString()).set(key.toStdString(), value.toStdString());
    emit changed(ns, key);
}

QString KvPlugin::get(const QString& ns, const QString& key) {
    auto result = backendForNamespace(ns.toStdString()).get(key.toStdString());
    if (!result)
        return {};
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
