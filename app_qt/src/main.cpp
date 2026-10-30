#include <cstdlib>

#include <QDir>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <Qt>
#include <QWindow>

#include "app_settings.h"
#include "mainverte_core.h"
#include "tools.h"
#include "translator.h"
#include "widget_wizard_setup.h"


namespace {
void on_object_creation_failed() {
    QCoreApplication::exit(-1);
}

void run_setup_wizard(QQmlApplicationEngine& engine, mv::Services& core_services);
}

int main(int argc, char** argv) {  // NOLINT(misc-include-cleaner)
    Stopwatch sw_app_init("app init");
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MainVerte"));

    const QString data_directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (data_directory.isEmpty()) {
        qCritical() << "Cannot determine the application data directory.";
        return EXIT_FAILURE;
    }
    const QString database_path = QDir(data_directory).filePath(QStringLiteral("database.db"));

    mv::Services core_services;
    if (!core_services.initialize(database_path.toStdString())) {
        qCritical() << "Cannot initialize Core services with database:" << database_path;
        return EXIT_FAILURE;
    }

    auto settings = mv::settings();

    ts::select_initial_language(settings.language);
    qInfo() << "ts selected language" << ts::current_language();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app,    &on_object_creation_failed,
                     Qt::QueuedConnection);
    auto* app_settings = new AppSettings(&engine);
    QObject::connect(app_settings, &AppSettings::languageChanged,
                     &engine,      &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), app_settings);
    if (settings.current_gardener_id > 0) {
        engine.loadFromModule("MainVerte", "Main");
    } else {
        run_setup_wizard(engine, core_services);
    }

    sw_app_init.~Stopwatch();

    return QGuiApplication::exec();
}

namespace {
void run_setup_wizard(QQmlApplicationEngine& engine, mv::Services& core_services) {
    auto* setup_wizard = new SetupWizard(core_services, &engine);
    QObject::connect(setup_wizard, &SetupWizard::languageChanged,
                     &engine,      &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("setupWizard"), setup_wizard);
    engine.loadFromModule("MainVerte", "SetupWindow");
    auto root_objects = engine.rootObjects();
    if (!root_objects.isEmpty()) {
        const QPointer<QWindow> wizard = qobject_cast<QWindow*>(root_objects.first());
        QObject::connect(setup_wizard, &SetupWizard::created, &engine, [&engine, wizard] {
            const auto previous_roots = engine.rootObjects();
            engine.loadFromModule("MainVerte", "Main");
            const auto roots = engine.rootObjects();
            if (!wizard || roots.size() <= previous_roots.size()) {
                return;
            }
            auto* main_window = qobject_cast<QWindow*>(roots.last());
            if (main_window == nullptr) {
                return;
            }

            const auto close_wizard = [wizard] {
                if (!wizard) {
                    return;
                }
                wizard->close();
                wizard->deleteLater();
            };
            if (main_window->isVisible()) {
                close_wizard();
            } else {
                QObject::connect(main_window, &QWindow::visibleChanged, wizard.data(),
                                 [close_wizard](bool visible) {
                    if (visible) {
                        close_wizard();
                    }
                });
            }
        });
    }
}

}
