#pragma once

#include <QObject>
#include <QVariantList>

class AppSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY themeChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
    Q_PROPERTY(bool saveFailed READ saveFailed NOTIFY stateChanged)

public:
    explicit AppSettings(QObject* parent = nullptr);

    [[nodiscard]] QVariantList  languages()     const;
    [[nodiscard]] int           languageIndex() const;
    [[nodiscard]] bool          saving()        const { return _saving; }
    [[nodiscard]] bool          saveFailed()    const { return _save_failed; }

    [[nodiscard]] bool darkTheme() const { return _dark_theme; }
    Q_INVOKABLE void selectTheme(bool dark);
    Q_INVOKABLE void selectLanguage(int index);

signals:
    void languageChanged();
    void themeChanged();
    void stateChanged();

private:
    bool _dark_theme  = false;
    bool _saving      = false;
    bool _save_failed = false;
};
