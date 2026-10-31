#pragma once

#include <cstdint>
#include <string>


namespace mv {

enum class LogLevel : int8_t;

struct Settings {
    static constexpr int64_t Undefined = 0;

    bool        dark_theme{false};
    LogLevel    log_level{};
    int64_t     current_gardener_id{Undefined};
    int64_t     current_collection_id{Undefined};
    std::string language;
};

}  // namespace mv
