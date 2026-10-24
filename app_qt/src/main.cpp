#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <Qt>


#include <mainverte_core.h>

namespace {
void on_object_creation_failed() {
    QCoreApplication::exit(-1);
}
}

int main(int argc, char** argv) {  // NOLINT(misc-include-cleaner)
    QGuiApplication app(argc, argv);

    core::initialize_platform({});

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app,    &on_object_creation_failed,
                     Qt::QueuedConnection);
    engine.loadFromModule("MainVerte", "Main");

    return QGuiApplication::exec();
}
