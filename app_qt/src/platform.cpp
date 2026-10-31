#include <mainverte_core.h>

#include <string_view>

#include <cstdlib>
#include <QGuiApplication>
#include <QMetaObject>

#include "mvlog/mvlog.h"


void mv::platform::log(Level severity, std::string_view msg) {
    switch (severity) {
    case Level::Dbg: mvlog::dbg(msg); break;
    case Level::Inf: mvlog::inf(msg); break;
    case Level::Wrn: mvlog::wrn(msg); break;
    case Level::Err: mvlog::err(msg); break;
    }
}

void mv::platform::dispatch_callback(mv::MoveOnlyFunction<void()> callback) {
    const bool queued = QMetaObject::invokeMethod(qApp, std::move(callback), Qt::QueuedConnection);
    if (!queued) {
        mvlog::err("PLATFORM: failed to enqueue core callback");
        std::abort();
    }
}
