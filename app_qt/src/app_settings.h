#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>


class AppSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList languages READ languages     CONSTANT)
    Q_PROPERTY(int languageIndex      READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme         READ darkTheme     NOTIFY themeChanged)
    Q_PROPERTY(bool debugEnabled READ debugEnabled NOTIFY debugEnabledChanged)
    Q_PROPERTY(QString softwareVersion READ softwareVersion CONSTANT)
    Q_PROPERTY(int databaseVersion READ databaseVersion CONSTANT)
    Q_PROPERTY(bool saving            READ saving        NOTIFY stateChanged)
    Q_PROPERTY(bool saveFailed        READ saveFailed    NOTIFY stateChanged)

public:
    explicit AppSettings(QObject* parent = nullptr);

    [[nodiscard]] QVariantList  languages()     const;
    [[nodiscard]] int           languageIndex() const;
    [[nodiscard]] bool          darkTheme()     const { return _dark_theme; }
    [[nodiscard]] bool          saving()        const { return _saving; }
    [[nodiscard]] bool          saveFailed()    const { return _save_failed; }

    [[nodiscard]] bool debugEnabled() const { return _debug_enabled; }
    [[nodiscard]] QString softwareVersion() const { return QStringLiteral("0.1.0"); }
    [[nodiscard]] int databaseVersion() const;
    Q_INVOKABLE void selectDebugEnabled(bool enabled);

    Q_INVOKABLE void selectTheme(bool dark);
    Q_INVOKABLE void selectLanguage(int index);
    Q_INVOKABLE void selectLogLevel(int index);

signals:
    void languageChanged();
    void themeChanged();
    void debugEnabledChanged();
    void stateChanged();

private:
    bool _debug_enabled = false;
    bool _dark_theme  = false;
    bool _saving      = false;
    bool _save_failed = false;

};
