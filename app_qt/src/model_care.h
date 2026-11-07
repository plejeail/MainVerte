#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

#include "mv_definitions.h"


struct CareNeedOption {
    Q_GADGET
    QML_VALUE_TYPE(careNeedOption)

    Q_PROPERTY(QString id MEMBER id CONSTANT)
    Q_PROPERTY(int type MEMBER type CONSTANT)
    Q_PROPERTY(QString name MEMBER name CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QUrl icon MEMBER icon CONSTANT)
    Q_PROPERTY(QString actionLabel MEMBER action_label CONSTANT)

public:
    QString id;
    int type = 0;
    QString name;
    QString description;
    QUrl icon;
    QString action_label;
};

class Care_Model : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(CareModel)
    Q_PROPERTY(QString specimenId READ specimen_id WRITE set_specimen_id NOTIFY changed)
    Q_PROPERTY(QList<CareNeedOption> needs READ needs  NOTIFY changed)
    Q_PROPERTY(QList<CareNeedOption> availableTypes READ available_types NOTIFY changed)
    Q_PROPERTY(bool    busy       READ busy   NOTIFY changed)
    Q_PROPERTY(bool    loaded     READ loaded NOTIFY changed)
    Q_PROPERTY(QString error      READ error  NOTIFY changed)

public:
    explicit Care_Model(QObject* parent = nullptr) : QObject(parent) {}

    void set_specimen_id(const QString& id);
    [[nodiscard]] QList<CareNeedOption> available_types() const;
    [[nodiscard]] QList<CareNeedOption> needs() const;
    [[nodiscard]] QString specimen_id() const { return _specimen_id; }
    [[nodiscard]] QString error()       const { return _error; }
    [[nodiscard]] bool loaded()         const { return _loaded; }
    [[nodiscard]] bool busy()           const { return _busy; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void add(int type);
    Q_INVOKABLE void remove(const QString& id);

signals:
    void changed();
    void saved();

private:
    void submit(mv::CareAction action);

    bool _busy   = false;
    bool _loaded = false;
    std::vector<mv::CareNeed> _needs;
    QString _specimen_id;
    QString _error;
};
