#pragma once

#include <QObject>
#include <QString>

#include "mainverte_core.h"

/// Creates and selects a gardener atomically through Core services.
class SetupWizard final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit SetupWizard(mv::Services& services, QObject* parent = nullptr)
        : QObject(parent), services_(services) {}

    [[nodiscard]] bool busy() const { return busy_; }
    Q_INVOKABLE void createGardener(const QString& name);

signals:
    void busyChanged();
    void created(const QString& name);
    void failed(const QString& message);

private:
    void setBusy(bool busy);

    mv::Services& services_;
    bool busy_ = false;
};
