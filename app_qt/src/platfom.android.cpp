#if (__ANDROID__)
#include "platform.h"

#if !defined(NDEBUG)
#include <csignal>
#include <cstring>
#endif

#include <android/log.h>

#include "mvlog.h"


void wait_for_debugger(int argc, char** argv) {
#if !defined(NDEBUG)
    // Qt forwards applicationArguments only for debuggable Android packages.
    // Pause before application initialization so LLDB can install breakpoints.
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--wait-for-native-debugger") == 0) {
            std::raise(SIGSTOP);
            break;
        }
    }
#endif
}

void platform_output(mvlog::Level level, const std::string& record) {
    int priority = ANDROID_LOG_INFO;
    switch (level) {
    case mvlog::Level::Debug:   priority = ANDROID_LOG_DEBUG; break;
    case mvlog::Level::Info:    priority = ANDROID_LOG_INFO; break;
    case mvlog::Level::Warning: priority = ANDROID_LOG_WARN; break;
    case mvlog::Level::Error:   priority = ANDROID_LOG_ERROR; break;
    }

    // Logcat is best-effort; the rotating file independently retains the full
    // record even when liblog filters or truncates a message.
    (void)::__android_log_write(priority, "MainVerte", record.c_str());
}

#endif
