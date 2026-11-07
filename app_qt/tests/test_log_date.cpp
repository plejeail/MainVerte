// clazy:skip
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include "../src/mvlog.h"


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

void set_eastern_zone() {
#ifdef _WIN32
    // UCRT's TZ path leaves _dstbias unchanged, including zero inherited from
    // restoring a system zone without DST. Supply the fixture's one-hour bias.
    _dstbias = -3600;
    set_zone("EST5EDT");
#else
    // Without explicit rules, POSIX leaves the transition dates to the runtime.
    set_zone("EST5EDT,M3.2.0/2,M11.1.0/2");
#endif
}

struct RestoreZone {
    const char* original{std::getenv("TZ")};
    bool had_zone{original != nullptr};
    std::string saved{had_zone ? original : ""};
#ifdef _WIN32
    long saved_dst_bias{};
    RestoreZone() {
        ::_tzset();
        saved_dst_bias = _dstbias;
    }
#endif

    ~RestoreZone() {
#ifdef _WIN32
        (void)::_putenv_s("TZ", saved.c_str());
        ::_tzset();
        _dstbias = saved_dst_bias;
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

}  //  namespace

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
#ifdef _WIN32
    // Reproduce the state left by _tzset() on the UTC Windows CI runner.
    _dstbias = 0;
#endif
    set_eastern_zone();
    mvlog::detail::LocalOffsetCache cache;
    std::time_t offset{};
    REQUIRE(cache.get(timestamp(2026, 1, 15, 12), offset));
    CHECK(offset == -5 * 3600);
    REQUIRE(cache.get(timestamp(2026, 7, 15, 12), offset));
    CHECK(offset == -4 * 3600);
}

TEST_CASE("Runtime local conversion and fresh offsets follow both daylight saving transitions") {
    RestoreZone restore;
    set_eastern_zone();
    const struct Sample {
        std::time_t utc;
        int hour;
        int minute;
        int second;
        int is_dst;
        std::time_t offset;
    } samples[]{
        {timestamp(2026, 3, 8, 6, 59, 59), 1, 59, 59, 0, -5 * 3600},
        {timestamp(2026, 3, 8, 7), 3, 0, 0, 1, -4 * 3600},
        {timestamp(2026, 11, 1, 5, 59, 59), 1, 59, 59, 1, -4 * 3600},
        {timestamp(2026, 11, 1, 6), 1, 0, 0, 0, -5 * 3600},
    };
    for (const auto& sample : samples) {
        CAPTURE(sample.utc);
        std::tm local{};
        REQUIRE(mvlog::detail::system_local_time(local, sample.utc));
        CHECK(local.tm_hour == sample.hour);
        CHECK(local.tm_min == sample.minute);
        CHECK(local.tm_sec == sample.second);
        CHECK(local.tm_isdst >= 0);
        CHECK((local.tm_isdst > 0) == (sample.is_dst > 0));
        // Each fresh cache samples the runtime at the requested instant.
        mvlog::detail::LocalOffsetCache cache;
        std::time_t offset{};
        REQUIRE(cache.get(sample.utc, offset));
        CHECK(offset == sample.offset);
    }
}

TEST_CASE("Daily cache defers daylight saving transitions until the next UTC day") {
    RestoreZone restore;
    set_eastern_zone();
    mvlog::detail::LocalOffsetCache cache;
    std::time_t offset{};
    REQUIRE(cache.get(timestamp(2026, 3, 8, 6, 59, 59), offset));
    CHECK(offset == -5 * 3600);
    REQUIRE(cache.get(timestamp(2026, 3, 8, 7), offset));
    CHECK(offset == -5 * 3600);
    REQUIRE(cache.get(timestamp(2026, 3, 9), offset));
    CHECK(offset == -4 * 3600);
    REQUIRE(cache.get(timestamp(2026, 11, 1, 5, 59, 59), offset));
    CHECK(offset == -4 * 3600);
    REQUIRE(cache.get(timestamp(2026, 11, 1, 6), offset));
    CHECK(offset == -4 * 3600);
    REQUIRE(cache.get(timestamp(2026, 11, 2), offset));
    CHECK(offset == -5 * 3600);
}

TEST_CASE("Zones without daylight saving keep the same winter and summer offsets") {
    RestoreZone restore;
    for (const char* zone : {"UTC0", "EST5"}) {
        CAPTURE(zone);
        set_zone(zone);
        mvlog::detail::LocalOffsetCache cache;
        for (const unsigned month : {1u, 7u}) {
            std::tm local{};
            REQUIRE(mvlog::detail::system_local_time(local, timestamp(2026, month, 15, 12)));
            CHECK(local.tm_isdst == 0);
            std::time_t offset{};
            REQUIRE(cache.get(timestamp(2026, month, 15, 12), offset));
            CHECK(offset == (std::string{zone} == "UTC0" ? 0 : -5 * 3600));
        }
    }
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
