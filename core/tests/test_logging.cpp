#include "tools.h"
#include "doctest.h"

#include <string>

namespace {
mv::Level logged_level;
std::string logged_message;
int log_count = 0;
}

void mv::Platform::log(Level severity, std::string_view message) {
    logged_level = severity;
    logged_message = message;
    ++log_count;
}

TEST_CASE("Formatted logs preserve severity and format arguments") {
    const std::string name = "plant";
    const int count = 3;
    log_count = 0;

    log_dbg("{} {}", name, count);
    CHECK(logged_level == mv::Level::Dbg);
    CHECK(logged_message == "plant 3");

    log_inf("{:.2f}", 1.25);
    CHECK(logged_level == mv::Level::Inf);
    CHECK(logged_message == "1.25");

    log_wrn("{{{}}}", std::string("warning"));
    CHECK(logged_level == mv::Level::Wrn);
    CHECK(logged_message == "{warning}");

    log_err("{:04d}", 7);
    CHECK(logged_level == mv::Level::Err);
    CHECK(logged_message == "0007");
    CHECK(log_count == 4);
}

TEST_CASE("Plain logs preserve literal braces and dynamic messages") {
    log_dbg("literal {}");
    CHECK(logged_level == mv::Level::Dbg);
    CHECK(logged_message == "literal {}");

    const std::string message = "dynamic {message}";
    log_inf(message);
    CHECK(logged_level == mv::Level::Inf);
    CHECK(logged_message == message);

    log_wrn(std::string_view(message));
    CHECK(logged_level == mv::Level::Wrn);
    CHECK(logged_message == message);

    log_err("literal {}");
    CHECK(logged_level == mv::Level::Err);
    CHECK(logged_message == "literal {}");
}
