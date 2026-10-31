#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "mv_core.h"


/// Creates and selects a gardener atomically through Core services.
class SetupWizard final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit SetupWizard(mv::Services& services, QObject* parent = nullptr)
        : QObject(parent), _services(services) {}

    [[nodiscard]] bool busy() const { return _busy; }
    [[nodiscard]] QVariantList languages() const;
    [[nodiscard]] int languageIndex() const;
    Q_INVOKABLE void selectLanguage(int index);
    Q_INVOKABLE void createGardener(const QString& name);

signals:
    void languageChanged();
    void busyChanged();
    void created(const QString& name);
    void failed(const QString& message);

private:
    void setBusy(bool busy);

    bool    _busy = false;
    mv::Services& _services;
    QString _chosen_language;
    QString _created_name;
};
