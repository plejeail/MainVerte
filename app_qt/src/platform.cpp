#include <mainverte_core.h>

#include <string_view>

#include <QDebug>
#include <QGuiApplication>
#include <QMetaObject>

void mv::platform::log(Level severity, std::string_view msg) {
    switch (severity) {
    case Level::Dbg: qDebug()    << "Dbg; " << msg; break;
    case Level::Inf: qInfo()     << "Inf; " << msg; break;
    case Level::Wrn: qWarning()  << "Wrn; " << msg; break;
    case Level::Err: qCritical() << "Err; " << msg; break;
    }
}

void mv::platform::dispatch_callback(mv::MoveOnlyFunction<void()> callback) {
    const bool queued = QMetaObject::invokeMethod(qApp, std::move(callback), Qt::QueuedConnection);
    if (!queued) {
        qFatal("PLATFORM: failed to enqueue core callback");
    }
}
