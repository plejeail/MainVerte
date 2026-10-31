#include "app_settings.h"

#include <cassert>
#include <cstddef>

#include <QObject>
#include <QPointer>
#include <QVariantList>

#include "mv_core.h"
#include "translator.h"


AppSettings::AppSettings(QObject* parent)
: QObject(parent)
, _debug_enabled(mv::settings().debug_enabled)
, _dark_theme(mv::settings().dark_theme)
{}

QVariantList AppSettings::languages() const {
    return ts::language_options();
}

int AppSettings::languageIndex() const {
    return ts::language_index();
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

void AppSettings::selectLogLevel(int index) {
    if (_saving) {
        return;
    }

    const auto value = static_cast<mv::LogLevel>(index);
    assert(value >= mv::LogLevel::Dbg && value <= mv::LogLevel::Err);

    auto settings = mv::settings();
    settings.log_level = value;
    _saving = true;
    _save_failed = false;
    emit stateChanged();
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

int AppSettings::databaseVersion() const {
    return mv::database_version();
}

void AppSettings::selectDebugEnabled(bool enabled) {
    if (_saving || enabled == _debug_enabled) {
        return;
    }

    auto settings = mv::settings();
    settings.debug_enabled = enabled;
    _debug_enabled = enabled;
    _saving = true;
    _save_failed = false;
    emit debugEnabledChanged();
    emit stateChanged();

    const QPointer<AppSettings> receiver(this);
    mv::save_settings(settings, [receiver](bool saved) {
        if (!receiver) {
            return;
        }

        receiver->_saving = false;
        receiver->_save_failed = !saved;
        if (!saved) {
            receiver->_debug_enabled = mv::settings().debug_enabled;
            emit receiver->debugEnabledChanged();
        }

        emit receiver->stateChanged();
    });
}
