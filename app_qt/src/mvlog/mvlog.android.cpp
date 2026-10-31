#include "mvlog.inl.h"

#include <android/log.h>

namespace mvlog::detail {
void platform_output(Level level, const std::string& record) {
    int priority = ANDROID_LOG_INFO;
    switch (level) {
    case Level::Debug:   priority = ANDROID_LOG_DEBUG; break;
    case Level::Info:    priority = ANDROID_LOG_INFO; break;
    case Level::Warning: priority = ANDROID_LOG_WARN; break;
    case Level::Error:   priority = ANDROID_LOG_ERROR; break;
    }

    // Logcat is best-effort; the rotating file independently retains the full
    // record even when liblog filters or truncates a message.
    (void)::__android_log_write(priority, "MainVerte", record.c_str());
}
}  // namespace mvlog::detail
