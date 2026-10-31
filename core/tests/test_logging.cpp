// clazy:skip
#include <string>
#include <string_view>

#include <doctest.h>

#include "tools.h"
#include "mv_core.h"


namespace {

mv::LogLevel logged_level;
std::string logged_message;
int log_count = 0;

}  // namespace

void mv::platform::log(LogLevel severity, std::string_view message) {
    logged_level = severity;
    logged_message = message;
    ++log_count;
}

TEST_CASE("Formatted logs preserve severity and format arguments") {
    const std::string name = "plant";
    const int count = 3;
    log_count = 0;

    log_dbg("{} {}", name, count);
    CHECK(logged_level == mv::LogLevel::Dbg);
    CHECK(logged_message == "plant 3");

    log_inf("{:.2f}", 1.25);
    CHECK(logged_level == mv::LogLevel::Inf);
    CHECK(logged_message == "1.25");

    log_wrn("{{{}}}", std::string("warning"));
    CHECK(logged_level == mv::LogLevel::Wrn);
    CHECK(logged_message == "{warning}");

    log_err("{:04d}", 7);
    CHECK(logged_level == mv::LogLevel::Err);
    CHECK(logged_message == "0007");
    CHECK(log_count == 4);
}

TEST_CASE("Plain logs preserve literal braces and dynamic messages") {
    log_dbg("literal {}");
    CHECK(logged_level == mv::LogLevel::Dbg);
    CHECK(logged_message == "literal {}");

    const std::string message = "dynamic {message}";
    log_inf(message);
    CHECK(logged_level == mv::LogLevel::Inf);
    CHECK(logged_message == message);

    log_wrn(std::string_view(message));
    CHECK(logged_level == mv::LogLevel::Wrn);
    CHECK(logged_message == message);

    log_err("literal {}");
    CHECK(logged_level == mv::LogLevel::Err);
    CHECK(logged_message == "literal {}");
}
