#pragma once

#include <cstdint>


namespace mv {
struct Settings {
    static constexpr int64_t Undefined = 0;

    int64_t current_gardener_id{Undefined};
    int64_t current_collection_id{Undefined};
    std::string language;
};
}  // namespace mv
