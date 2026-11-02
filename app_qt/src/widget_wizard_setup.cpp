#include "widget_wizard_setup.h"

#include <cstddef>

#include <QDebug>
#include <QPointer>
#include <QVariantList>

#include "mv_core.h"
#include "mv_definitions.h"
#include "mvlog/mvlog.h"
#include "translator.h"


QVariantList SetupWizard::languages() const {
    return ts::language_options();
}

int SetupWizard::languageIndex() const {
    return ts::language_index();
}

void SetupWizard::selectLanguage(int index) {
    const auto codes = ts::list_languages();
    if (_busy || index < 0 || static_cast<std::size_t>(index) >= codes.size()) {
        return;
    }

    const auto code = codes[index];
    if (ts::select_language(code)) {
        _chosen_language = QString::fromUtf8(code.data(), static_cast<int>(code.size()));
        emit languageChanged();
    }
}

void SetupWizard::complete_setup(const QString& name) {
    if (_busy) {
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
    mv::complete_setup(trimmed_name.toStdString(), _chosen_language.toStdString(),
    [receiver](mv::GardenerResult result) {
            if (!receiver) {
                return;
            }

            receiver->setBusy(false);
            if (result) {
                emit receiver->created(QString::fromStdString(result->name));
                return;
            }

            mvlog::wrn("Gardener creation failed: {}", result.error().message);
            if (result.error().code == mv::MainverteError::Code::NotRunning) {
                emit receiver->failed(ts::get(ts::TextId::Gardener_ServiceUnavailable));
            } else {
                emit receiver->failed(ts::get(ts::TextId::Gardener_CreateFailed));
            }
        }
    );
}

void SetupWizard::setBusy(bool busy) {
    if (_busy != busy) {
        _busy = busy;
        emit busyChanged();
    }
}
