#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>


struct LanguageOption {
    Q_GADGET
    QML_VALUE_TYPE(languageOption)

    Q_PROPERTY(QString code  MEMBER code  CONSTANT)
    Q_PROPERTY(QString label MEMBER label CONSTANT)
    Q_PROPERTY(QString flag  MEMBER flag  CONSTANT)

public:
    QString code;
    QString label;
    QString flag;
};
