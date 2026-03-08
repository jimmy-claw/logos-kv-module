#pragma once

#include "kv_interface.h"
#include "backends/KvBackend.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class KvPlugin : public KvInterface {
    Q_OBJECT

public:
    explicit KvPlugin(QObject *parent = nullptr);
    ~KvPlugin() override = default;

    void setDataDir(const QString &path);

public slots:
    void set(QString ns, QString key, QByteArray value) override;
    QByteArray get(QString ns, QString key) override;
    void remove(QString ns, QString key) override;
    QStringList list(QString ns, QString prefix) override;
    void clear(QString ns) override;

private:
    KvBackend &backendForNamespace(const std::string &ns);

    QString data_dir_;
    std::mutex backends_mutex_;
    std::unordered_map<std::string, std::unique_ptr<KvBackend>> backends_;
    bool use_file_backend_ = false;
};
