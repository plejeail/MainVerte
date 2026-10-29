#include "widget_wizard_setup.h"

#include <QDebug>
#include <QPointer>

#include "translator.h"


void SetupWizard::createGardener(const QString& name) {
    if (busy_) {
        return;
    }

    const QString trimmed_name = name.trimmed();
    if (trimmed_name.isEmpty()) {
        emit failed(ts::get(ts::TextId::Gardener_NameRequired));
        return;
    }

    if (trimmed_name.size() > 30) {
        emit failed(ts::get(ts::TextId::Gardener_NameTooLong));
        return;
    }
    setBusy(true);

    // Core delivers the completion through platform::dispatch_callback on the
    // Qt event thread. The guarded pointer handles window/application teardown.
    const QPointer<SetupWizard> receiver(this);
    mv::create_and_select_gardener(trimmed_name.toUtf8().toStdString(),
                        [receiver](mv::GardenerResult result) {
        if (!receiver) {
            return;
        }

        receiver->setBusy(false);
        if (result) {
            emit receiver->created(QString::fromStdString(result->name));
            return;
        }

        qWarning() << "Gardener creation failed:" << QString::fromStdString(result.error().message);
        if (result.error().code == mv::MainverteError::Code::NotRunning) {
            emit receiver->failed(ts::get(ts::TextId::Gardener_ServiceUnavailable));
        } else {
            emit receiver->failed(ts::get(ts::TextId::Gardener_CreateFailed));
        }
    });
}

void SetupWizard::setBusy(bool busy) {
    if (busy_ != busy) {
        busy_ = busy;
        emit busyChanged();
    }
}
