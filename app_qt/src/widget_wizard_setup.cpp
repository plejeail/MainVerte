#include "widget_wizard_setup.h"

#include <QDebug>
#include <QPointer>

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
        _chosen_language = QString::fromUtf8(code.data(), code.size());
        emit languageChanged();
    }
}

void SetupWizard::createGardener(const QString& name) {
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
    const auto save_selection = [receiver](const QString& gardener_name) {
        auto settings = mv::settings();
        if (!receiver->_chosen_language.isEmpty()) {
            settings.language = receiver->_chosen_language.toStdString();
        }

        mv::save_settings(settings, [receiver, gardener_name](bool saved) {
            if (!receiver) {
                return;
            }

            receiver->setBusy(false);
            if (saved) {
                emit receiver->created(gardener_name);
            } else {
                emit receiver->failed(ts::get(ts::TextId::Gardener_SaveFailed));
            }
        });
    };

    // Retry a failed settings write without creating a second gardener.
    if (!_created_name.isEmpty()) {
        save_selection(_created_name);
        return;
    }

    mv::create_and_select_gardener(trimmed_name.toStdString(),
                        [receiver, save_selection](mv::GardenerResult result) {
        if (!receiver) {
            return;
        }

        if (result) {
            receiver->_created_name = QString::fromStdString(result->name);
            save_selection(receiver->_created_name);
            return;
        }

        receiver->setBusy(false);
        qWarning() << "Gardener creation failed:" << QString::fromStdString(result.error().message);
        if (result.error().code == mv::MainverteError::Code::NotRunning) {
            emit receiver->failed(ts::get(ts::TextId::Gardener_ServiceUnavailable));
        } else {
            emit receiver->failed(ts::get(ts::TextId::Gardener_CreateFailed));
        }
    });
}

void SetupWizard::setBusy(bool busy) {
    if (_busy != busy) {
        _busy = busy;
        emit busyChanged();
    }
}
