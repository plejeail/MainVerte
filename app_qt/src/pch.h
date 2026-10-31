#pragma once

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariantList>
#include <QVariantMap>
#include <QWindow>
#include <Qt>
#include <QtContainerFwd>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQmlIntegration/qqmlintegration.h>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif
