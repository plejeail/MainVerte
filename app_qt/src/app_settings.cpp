#include "app_settings.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include <QObject>
#include <QPointer>
#include <QVariantList>

#include "mv_core.h"
#include "mvlog/mvlog.h"
#include "translator.h"


namespace {

constexpr std::array LogLevels{
    mvlog::Level::Debug, mvlog::Level::Info, mvlog::Level::Warning, mvlog::Level::Error};

}  //  namespace

AppSettings::AppSettings(QObject* parent)
: QObject(parent)
, _log_level(static_cast<int>(mv::settings().log_level))
, _debug_enabled(mv::settings().debug_enabled)
, _dark_theme(mv::settings().dark_theme)
{
    mvlog::set_level(LogLevels[_log_level]);
}

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

    const std::string previous_language(ts::current_language());
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
    mv::save_settings(settings,
    [receiver, previous_language=std::move(previous_language)](bool saved) {
        if (!receiver) {
            return;
        }

        if (!saved) {
            // Core retains the saved preference on failure, including an empty
            // system preference. Restore the effective language independently.
            ts::select_language(previous_language);
            emit receiver->languageChanged();
        }

        receiver->_saving = false;
        receiver->_save_failed = !saved;
        emit receiver->stateChanged();
    });
}

void AppSettings::selectLogLevel(int index) {
    if (_saving || index < 0 || static_cast<std::size_t>(index) >= LogLevels.size() || index == _log_level) {
        return;
    }

    const auto previous_level = mvlog::level();
    const int previous_index = _log_level;
    auto settings = mv::settings();
    settings.log_level = static_cast<mv::LogLevel>(index);
    _log_level = index;
    mvlog::set_level(LogLevels[index]);
    _saving = true;
    _save_failed = false;
    emit logLevelChanged();
    emit stateChanged();
    const QPointer<AppSettings> receiver(this);
    mv::save_settings(settings, [receiver, previous_level, previous_index](bool saved) {
        if (!saved) {
            mvlog::set_level(previous_level);
        }

        if (!receiver) {
            return;
        }

        if (!saved) {
            receiver->_log_level = previous_index;
            emit receiver->logLevelChanged();
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
