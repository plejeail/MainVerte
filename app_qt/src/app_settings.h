#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "language_option.h"


class App_Settings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<LanguageOption> languages READ languages     CONSTANT)
    Q_PROPERTY(int languageIndex   READ language_index NOTIFY language_changed)
    Q_PROPERTY(bool darkTheme      READ dark_theme     NOTIFY theme_changed)
    Q_PROPERTY(bool debugEnabled   READ debug_enabled  NOTIFY debug_enabled_changed)
    Q_PROPERTY(QString softwareVersion READ software_version CONSTANT)
    Q_PROPERTY(int databaseVersion READ databaseVersion      CONSTANT)
    Q_PROPERTY(int logLevel        READ log_level      NOTIFY log_level_changed)
    Q_PROPERTY(bool saving         READ saving         NOTIFY state_changed)
    Q_PROPERTY(bool saveFailed     READ save_failed    NOTIFY state_changed)

public:
    explicit App_Settings(QObject* parent = nullptr);

    [[nodiscard]] QList<LanguageOption> languages() const;
    [[nodiscard]] int  language_index() const;
    [[nodiscard]] bool dark_theme()     const { return _dark_theme_enabled; }
    [[nodiscard]] int  log_level()      const { return _log_level; }
    [[nodiscard]] bool saving()         const { return _saving; }
    [[nodiscard]] bool save_failed()    const { return _save_failed; }
    [[nodiscard]] bool debug_enabled()  const { return _debug_enabled; }
    [[nodiscard]] QString software_version() const { return QStringLiteral("0.1.0"); }
    [[nodiscard]] int databaseVersion() const;

    Q_INVOKABLE void select_debug_enabled(bool enabled);
    Q_INVOKABLE void select_theme(bool dark_theme_enabled);
    Q_INVOKABLE void select_language(int index);
    Q_INVOKABLE void select_log_level(int index);

signals:
    void language_changed();
    void theme_changed();
    void debug_enabled_changed();
    void log_level_changed();
    void state_changed();

private:
    int _log_level = 0;
    bool _debug_enabled = false;
    bool _dark_theme_enabled  = false;
    bool _saving      = false;
    bool _save_failed = false;

};
