#include "app_settings.h"

#include <QPointer>

#include "mainverte_core.h"
#include "translator.h"

AppSettings::AppSettings(QObject* parent) : QObject(parent), _dark_theme(mv::settings().dark_theme) {}

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

void AppSettings::selectTheme(bool dark) {
    if (_saving || dark == _dark_theme) {
        return;
    }
    auto settings = mv::settings();
    settings.dark_theme = dark;
    _dark_theme = dark;
    _saving = true;
    _save_failed = false;
    emit themeChanged();
    emit stateChanged();

    const QPointer<AppSettings> receiver(this);
    mv::save_settings(settings, [receiver](bool saved) {
        if (!receiver) {
            return;
        }
        receiver->_saving = false;
        receiver->_save_failed = !saved;
        if (!saved) {
            receiver->_dark_theme = mv::settings().dark_theme;
            emit receiver->themeChanged();
        }
        emit receiver->stateChanged();
    });
}
