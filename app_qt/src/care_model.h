#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

#include "mv_care.h"

class CareModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString specimenId READ specimenId WRITE setSpecimenId NOTIFY changed)
    Q_PROPERTY(QVariantList needs READ needs NOTIFY changed)
    Q_PROPERTY(QVariantList availableTypes READ availableTypes NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit CareModel(QObject* parent = nullptr) : QObject(parent) {}
    QString specimenId() const { return _specimenId; }
    void setSpecimenId(const QString& id);
    QVariantList needs() const;
    QVariantList availableTypes() const;
    bool busy() const { return _busy; }
    bool loaded() const { return _loaded; }
    QString error() const { return _error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void add(int type);
    Q_INVOKABLE void remove(const QString& id);
signals:
    void changed();
    void saved();
private:
    void submit(mv::CareAction action);
    QString _specimenId;
    std::vector<mv::CareNeed> _needs;
    bool _busy = false;
    bool _loaded = false;
    QString _error;
};
