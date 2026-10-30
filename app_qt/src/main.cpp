#include <cstdlib>

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <Qt>
#include <QtQml/QQmlExtensionPlugin>
#include <QWindow>

#include "app_settings.h"
#include "mainverte_core.h"
#include "tools.h"
#include "translator.h"
#include "widget_wizard_setup.h"

#ifdef Q_OS_ANDROID
#include "setup_system_bars.android.h"
#endif


Q_IMPORT_QML_PLUGIN(MainVerteStylePlugin)
Q_IMPORT_QML_PLUGIN(MainVerteShowcasePlugin)

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
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/img/logo_mainverte.png")));
    QQuickStyle::setStyle(QStringLiteral("MainVerteStyle"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));

    // The gallery is isolated from Core initialization and never opens the journal database.
    if (app.arguments().contains(QStringLiteral("--showcase"))) {
        QQmlApplicationEngine showcase_engine;
        QObject::connect(&showcase_engine, &QQmlApplicationEngine::objectCreationFailed,
                         &app, &on_object_creation_failed, Qt::QueuedConnection);
        showcase_engine.loadFromModule("MainVerte.Showcase", "Showcase");
        return QGuiApplication::exec();
    }

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
    auto* tokens = engine.singletonInstance<QObject*>("MainVerteStyle", "Tokens");
    if (tokens == nullptr) {
        qCritical() << "Cannot load style tokens.";
        return EXIT_FAILURE;
    }
    tokens->setProperty("dark", app_settings->darkTheme());
    QObject::connect(app_settings, &AppSettings::themeChanged, tokens, [tokens, app_settings] {
        tokens->setProperty("dark", app_settings->darkTheme());
    });
    if (settings.current_gardener_id > 0) {
        engine.loadFromModule("MainVerte", "Main");
    } else {
        run_setup_wizard(engine, core_services);
    }
#ifdef Q_OS_ANDROID
    const auto roots = engine.rootObjects();
    auto* setup_window = settings.current_gardener_id <= 0 && !roots.isEmpty()
        ? qobject_cast<QWindow*>(roots.first()) : nullptr;
    configure_app_system_bars(*app_settings, setup_window);
#endif

    sw_app_init.stop();

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
