#include "../src/mvlog/mvlog.inl.h"

#include <android/log.h>
#include <doctest/doctest.h>

namespace {
int captured_priority{};
std::string captured_tag;
std::string captured_text;
}

extern "C" int __android_log_write(int priority, const char* tag, const char* text) {
    captured_priority = priority;
    captured_tag = tag;
    captured_text = text;
    return 1;
}

TEST_CASE("Android output preserves the formatted record and maps priorities") {
    std::tm date{};
    date.tm_mon = 1;
    date.tm_mday = 23;
    date.tm_hour = 12;
    date.tm_min = 4;
    date.tm_sec = 56;
    struct Example {
        mvlog::Level level;
        int priority;
    };
    for (const auto example : {
             Example{mvlog::Level::Debug, ANDROID_LOG_DEBUG},
             Example{mvlog::Level::Info, ANDROID_LOG_INFO},
             Example{mvlog::Level::Warning, ANDROID_LOG_WARN},
             Example{mvlog::Level::Error, ANDROID_LOG_ERROR}}) {
        const auto record = mvlog::detail::format_record(static_cast<char>(example.level), date, "ding dong");
        mvlog::detail::platform_output(example.level, record);
        CHECK(captured_priority == example.priority);
        CHECK(captured_tag == "MainVerte");
        CHECK(captured_text == record);
        CHECK(captured_text.substr(1) == " 0223 12:04:56 ding dong\n");
    }
}
