#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QStringList>

class KvInterface : public QObject {
    Q_OBJECT

public:
    explicit KvInterface(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~KvInterface() = default;

public slots:
    virtual void set(QString ns, QString key, QByteArray value) = 0;
    virtual QByteArray get(QString ns, QString key) = 0;
    virtual void remove(QString ns, QString key) = 0;
    virtual QStringList list(QString ns, QString prefix) = 0;
    virtual void clear(QString ns) = 0;

signals:
    void changed(QString ns, QString key);
};
