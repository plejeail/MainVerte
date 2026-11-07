#include "app_settings.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include <QList>
#include <QObject>
#include <QPointer>

#include "mv_core.h"
#include "mvlog.h"
#include "translator.h"


namespace {

constexpr std::array LogLevels{
    mvlog::Level::Debug,
    mvlog::Level::Info,
    mvlog::Level::Warning,
    mvlog::Level::Error,
};

}  //  namespace

App_Settings::App_Settings(QObject* parent)
: QObject(parent)
, _log_level(static_cast<int>(mv::settings().log_level))
, _debug_enabled(mv::settings().debug_enabled)
, _dark_theme_enabled(mv::settings().dark_theme_enabled) {
    mvlog::set_level(LogLevels[_log_level]);
}

QList<LanguageOption> App_Settings::languages() const {
    return ts::language_options();
}

int App_Settings::language_index() const {
    return ts::language_index();
}

void App_Settings::select_theme(bool dark_theme_enabled) {
    if (_saving || dark_theme_enabled == _dark_theme_enabled) {
        return;
    }

    auto settings = mv::settings();
    settings.dark_theme_enabled = dark_theme_enabled;
    _dark_theme_enabled = dark_theme_enabled;
    _saving = true;
    _save_failed = false;

    const QPointer<App_Settings> receiver(this);
    mv::save_settings(settings, [receiver](bool saved) {
        if (!receiver) {
            return;
        }

        receiver->_saving = false;
        receiver->_save_failed = !saved;
        if (saved) {
            receiver->_dark_theme_enabled = mv::settings().dark_theme_enabled;
            emit receiver->theme_changed();
        }

        emit receiver->state_changed();
    });
}

void App_Settings::select_language(int index) {
    const auto codes = ts::list_languages();
    if (_saving || index < 0 || static_cast<std::size_t>(index) >= codes.size()) {
        return;
    }

    std::string previous_language(ts::current_language());
    auto settings = mv::settings();
    settings.language = codes[index];
    if (!ts::select_language(settings.language)) {
        return;
    }

    _saving = true;
    _save_failed = false;

    const QPointer<App_Settings> receiver(this);
    mv::save_settings(settings,
    [receiver, previous_language=std::move(previous_language)](bool saved) {
        if (!receiver) {
            return;
        }

        if (saved) {
            emit receiver->state_changed();
            emit receiver->language_changed();
        } else {
            // Core retains the saved preference on failure, including an empty
            // system preference. Restore the effective language independently.
            ts::select_language(previous_language);
        }

        receiver->_saving = false;
        receiver->_save_failed = !saved;
        emit receiver->state_changed();
    });
}

void App_Settings::select_log_level(int index) {
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
    emit log_level_changed();
    emit state_changed();
    const QPointer<App_Settings> receiver(this);
    mv::save_settings(settings,
    [receiver, previous_level, previous_index](bool saved) {
        if (!saved) {
            mvlog::set_level(previous_level);
        }

        if (!receiver) {
            return;
        }

        if (!saved) {
            receiver->_log_level = previous_index;
            emit receiver->log_level_changed();
        }

        receiver->_saving = false;
        receiver->_save_failed = !saved;
        emit receiver->state_changed();
    });
}

int App_Settings::databaseVersion() const {
    return mv::database_version();
}

void App_Settings::select_debug_enabled(bool enabled) {
    if (_saving || enabled == _debug_enabled) {
        return;
    }

    auto settings = mv::settings();
    settings.debug_enabled = enabled;
    _debug_enabled = enabled;
    _saving = true;
    _save_failed = false;
    emit debug_enabled_changed();
    emit state_changed();

    const QPointer<App_Settings> receiver(this);
    mv::save_settings(settings,
    [receiver](bool saved) {
        if (!receiver) {
            return;
        }

        receiver->_saving = false;
        receiver->_save_failed = !saved;
        if (!saved) {
            receiver->_debug_enabled = mv::settings().debug_enabled;
            emit receiver->debug_enabled_changed();
        }

        emit receiver->state_changed();
    });
}
