#include <doctest/doctest.h>

#include "../src/mvlog/mvlog.inl.h"

#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

namespace {
std::time_t timestamp(int year, unsigned month, unsigned day, int hour = 0, int minute = 0, int second = 0) {
    using namespace std::chrono;
    return static_cast<std::time_t>((sys_days{std::chrono::year{year} / month / day} +
                                    hours{hour} + minutes{minute} + seconds{second}).time_since_epoch().count());
}

void set_zone(const char* zone) {
#ifdef _WIN32
    REQUIRE(::_putenv_s("TZ", zone) == 0);
    ::_tzset();
#else
    REQUIRE(::setenv("TZ", zone, 1) == 0);
    ::tzset();
#endif
}

struct RestoreZone {
    const char* original{std::getenv("TZ")};
    bool had_zone{original != nullptr};
    std::string saved{had_zone ? original : ""};
    ~RestoreZone() {
#ifdef _WIN32
        (void)::_putenv_s("TZ", saved.c_str());
        ::_tzset();
#else
        if (had_zone) {
            (void)::setenv("TZ", saved.c_str(), 1);
        } else {
            (void)::unsetenv("TZ");
        }
        ::tzset();
#endif
    }
};
}

TEST_CASE("UTC conversion handles late hours, leap days and century boundaries") {
    std::tm date{};
    REQUIRE(mvlog::detail::utc_time(date, 0));
    CHECK(date.tm_year == 70);
    CHECK(date.tm_mon == 0);
    CHECK(date.tm_mday == 1);
    REQUIRE(mvlog::detail::utc_time(date, timestamp(2024, 2, 29, 23, 59, 59)));
    CHECK(date.tm_year == 124);
    CHECK(date.tm_mon == 1);
    CHECK(date.tm_mday == 29);
    CHECK(date.tm_hour == 23);
    CHECK(date.tm_min == 59);
    CHECK(date.tm_sec == 59);
    REQUIRE(mvlog::detail::utc_time(date, timestamp(2100, 3, 1)));
    CHECK(date.tm_year == 200);
    CHECK(date.tm_yday == 59);
}

TEST_CASE("Offset cache refreshes at UTC midnight and after a backwards day jump") {
    RestoreZone restore;
    set_zone("UTC0");
    mvlog::detail::LocalOffsetCache cache;
    std::time_t offset{};
    REQUIRE(cache.get(timestamp(2026, 1, 1, 1), offset));
    CHECK(offset == 0);
    set_zone("EST5");
    REQUIRE(cache.get(timestamp(2026, 1, 1, 23, 59, 59), offset));
    CHECK(offset == 0);
    REQUIRE(cache.get(timestamp(2026, 1, 2), offset));
    CHECK(offset == -5 * 3600);
    set_zone("UTC0");
    REQUIRE(cache.get(timestamp(2026, 1, 1), offset));
    CHECK(offset == 0);
}

TEST_CASE("Local conversion handles year rollover and concurrent callers") {
    RestoreZone restore;
    set_zone("EST5");
    const auto time = timestamp(2026, 1, 1, 1);
    std::tm date{};
    REQUIRE(mvlog::detail::local_time(date, time));
    CHECK(date.tm_year == 125);
    CHECK(date.tm_mon == 11);
    CHECK(date.tm_mday == 31);
    CHECK(date.tm_hour == 20);
    CHECK(date.tm_isdst == -1);
    std::atomic<bool> valid{true};
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back([&] {
            for (int j = 0; j < 1000; ++j) {
                std::tm result{};
                if (!mvlog::detail::local_time(result, time) || result.tm_hour != 20) {
                    valid = false;
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    CHECK(valid.load());
}

TEST_CASE("Two digit formatting writes all values without touching surrounding bytes") {
    for (unsigned value = 0; value <= 99; ++value) {
        char buffer[]{'x', 'x', 'x', 'x'};
        mvlog::detail::copy_time(buffer + 1, value);
        CHECK(buffer[0] == 'x');
        CHECK(buffer[1] == static_cast<char>('0' + value / 10));
        CHECK(buffer[2] == static_cast<char>('0' + value % 10));
        CHECK(buffer[3] == 'x');
    }
}

TEST_CASE("Daily offset calculation includes daylight saving without a manual correction") {
    RestoreZone restore;
    set_zone("EST5EDT");
    mvlog::detail::LocalOffsetCache cache;
    std::time_t offset{};
    REQUIRE(cache.get(timestamp(2026, 1, 15, 12), offset));
    CHECK(offset == -5 * 3600);
    REQUIRE(cache.get(timestamp(2026, 7, 15, 12), offset));
    CHECK(offset == -4 * 3600);
}

TEST_CASE("Failed conversion leaves the output date unchanged") {
    std::tm date{};
    date.tm_year = 126;
    CHECK_FALSE(mvlog::detail::local_time(date, std::numeric_limits<std::time_t>::max()));
    CHECK(date.tm_year == 126);
}

TEST_CASE("Log record has a fixed level, MMDD and HH:MM:SS prefix") {
    std::tm date{};
    date.tm_mon = 1;
    date.tm_mday = 23;
    date.tm_hour = 12;
    date.tm_min = 4;
    date.tm_sec = 56;
    CHECK(mvlog::detail::format_record('D', date, "ding dong") == "D 0223 12:04:56 ding dong\n");
    for (const char level : {'D', 'I', 'W', 'E'}) {
        const auto record = mvlog::detail::format_record(level, date, "");
        CHECK(record[0] == level);
        CHECK(record.substr(1) == " 0223 12:04:56 \n");
    }
    date.tm_mon = 11;
    date.tm_mday = 31;
    date.tm_hour = 23;
    date.tm_min = 59;
    date.tm_sec = 59;
    CHECK(mvlog::detail::format_record('E', date, "last") == "E 1231 23:59:59 last\n");
}
