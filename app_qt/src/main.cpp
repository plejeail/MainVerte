#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <Qt>

#include <cstdlib>

#include "mainverte_core.h"


namespace {
void on_object_creation_failed() {
    QCoreApplication::exit(-1);
}
}

int main(int argc, char** argv) {  // NOLINT(misc-include-cleaner)
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MainVerte"));

    const QString data_directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (data_directory.isEmpty()) {
        qCritical() << "Cannot determine the application data directory.";
        return EXIT_FAILURE;
    }
    const QString database_path = QDir(data_directory).filePath(QStringLiteral("database.db"));

    mv::Services core_services;
    if (!core_services.initialize(database_path.toUtf8().toStdString())) {
        qCritical() << "Cannot initialize Core services with database:" << database_path;
        return EXIT_FAILURE;
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app,    &on_object_creation_failed,
                     Qt::QueuedConnection);
    engine.loadFromModule("MainVerte", "Main");

    return QGuiApplication::exec();
}
