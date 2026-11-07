// clazy:skip
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "../src/mvlog.h"


namespace {

std::vector<mvlog::Level> emitted;

}  //  namespace

void mvlog::detail::write(Level level, std::string_view) {
    emitted.push_back(level);
}

TEST_CASE("Compile-time log switches override the runtime threshold") {
    mvlog::set_level(mvlog::Level::Debug);
    mvlog::dbg("plain debug");
    mvlog::dbg("formatted debug {}", 1);
    mvlog::inf("plain info");
    mvlog::inf("formatted info {}", 1);
    mvlog::wrn("plain warning");
    mvlog::wrn("formatted warning {}", 1);
    mvlog::err("plain error");
    mvlog::err("formatted error {}", 1);
    const std::vector levels{mvlog::Level::Debug, mvlog::Level::Info, mvlog::Level::Warning, mvlog::Level::Error};
    const std::vector compiled{MV_LOG_ENABLE_DBG, MV_LOG_ENABLE_INF, MV_LOG_ENABLE_WRN, MV_LOG_ENABLE_ERR};
    std::size_t position = 0;
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const bool enabled = !MV_LOG_DISABLED && compiled[i];
        CHECK(mvlog::enabled(levels[i]) == enabled);
        if (enabled) {
            REQUIRE(position + 1 < emitted.size());
            CHECK(emitted[position++] == levels[i]);
            CHECK(emitted[position++] == levels[i]);
        }
    }

    CHECK(emitted.size() == position);
}
