#include <mv_core.h>

#include <cstdlib>
#include <string_view>

#include <QGuiApplication>
#include <QMetaObject>

#include "mvlog/mvlog.h"
#include "mv_move_only_function.h"


void mv::platform::log(LogLevel severity, std::string_view msg) {
    switch (severity) {
    case LogLevel::Dbg: mvlog::dbg(msg); break;
    case LogLevel::Inf: mvlog::inf(msg); break;
    case LogLevel::Wrn: mvlog::wrn(msg); break;
    case LogLevel::Err: mvlog::err(msg); break;
    }
}

void mv::platform::dispatch_callback(mv::MoveOnlyFunction<void()> callback) {
    const bool queued = QMetaObject::invokeMethod(qApp, std::move(callback), Qt::QueuedConnection);
    if (!queued) {
        mvlog::err("PLATFORM: failed to enqueue core callback");
        std::abort();
    }
}
