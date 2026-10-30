#pragma once

#include <QObject>
#include <QVariantList>

class AppSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
    Q_PROPERTY(bool saveFailed READ saveFailed NOTIFY stateChanged)

public:
    explicit AppSettings(QObject* parent = nullptr) : QObject(parent) {}

    [[nodiscard]] QVariantList  languages()     const;
    [[nodiscard]] int           languageIndex() const;
    [[nodiscard]] bool          saving()        const { return _saving; }
    [[nodiscard]] bool          saveFailed()    const { return _save_failed; }

    Q_INVOKABLE void selectLanguage(int index);

signals:
    void languageChanged();
    void stateChanged();

private:
    bool _saving      = false;
    bool _save_failed = false;
};
