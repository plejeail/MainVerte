#pragma once

#include <cstdint>
#include <expected>
#include <string>


namespace mv {

/// internal object id type
using id_t = int64_t;

struct MainverteError {
    enum class Code : uint8_t { NotRunning, Database };
    Code code;
    std::string message;
};

struct Gardener {
    id_t id;
    std::string name;
};

using GardenerResult = std::expected<Gardener, MainverteError>;

}  // namespace mv
