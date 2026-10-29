#pragma once

#include <cstdint>


namespace mv {
struct Settings {
    // Zero denotes no selection (SQL NULL).
    int64_t current_gardener_id{0};
    int64_t current_collection_id{0};
};
}  // namespace mv
