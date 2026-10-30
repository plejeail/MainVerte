#include "app_settings.h"

#include <QPointer>

#include "mainverte_core.h"
#include "translator.h"

QVariantList AppSettings::languages() const {
    return ts::language_options();
}

int AppSettings::languageIndex() const {
    return ts::language_index();
}

void AppSettings::selectLanguage(int index) {
    const auto codes = ts::list_languages();
    if (_saving || index < 0 || static_cast<std::size_t>(index) >= codes.size()) {
        return;
    }
    auto settings = mv::settings();
    settings.language = codes[index];
    if (!ts::select_language(settings.language)) {
        return;
    }
    _saving = true;
    _save_failed = false;
    emit stateChanged();
    emit languageChanged();

    const QPointer<AppSettings> receiver(this);
    mv::save_settings(settings, [receiver](bool saved) {
        if (!receiver) {
            return;
        }
        receiver->_saving = false;
        receiver->_save_failed = !saved;
        emit receiver->stateChanged();
    });
}
