#ifndef I_KV_MODULE_H
#define I_KV_MODULE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QStringList>
#ifdef LOGOS_CORE_AVAILABLE
#include <interface.h>
#endif

/**
 * IKvModule — Public interface for the KV Storage Logos Core module.
 *
 * Exposes key-value storage operations as Q_INVOKABLE Qt methods
 * for the Logos Core inter-module RPC system.
 */
class IKvModule {
public:
    virtual ~IKvModule() = default;

    // ── KV Operations ────────────────────────────────────────────────────────

    virtual void set(const QString& ns, const QString& key, const QString& value) = 0;
    virtual QString get(const QString& ns, const QString& key) = 0;
    virtual void remove(const QString& ns, const QString& key) = 0;
    virtual QString list(const QString& ns, const QString& prefix) = 0;
    virtual QString listAll(const QString& ns) = 0;
    virtual void clear(const QString& ns) = 0;
};

#define IKvModule_iid "com.logos.module.IKvModule"
Q_DECLARE_INTERFACE(IKvModule, IKvModule_iid)

#endif // I_KV_MODULE_H
