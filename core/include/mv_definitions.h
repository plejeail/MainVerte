// include/mv_definitions.h ---------------------------------------------------
// MAINVERTE_CORE DOMAIN DEFINITIONS
//
// Domain values and explicit operation results shared with the application.
//
//-------------------------------------------------------------- mainverte/core
#pragma once

#include <cstdint>
#include <expected>
#include <string>


namespace mv {

/// Database object identifier; persisted objects have positive IDs.
/// Settings use zero to represent an absent selection.
using id_t = int64_t;

/// Operation failure: inspect code for handling and message for diagnostics.
struct MainverteError {
    enum class Code : uint8_t {
        NotRunning,  ///< The database worker refused the submission.
        Database     ///< A database operation failed after submission.
    };

    Code code;
    /// Owned diagnostic text; not a stable identifier or a localized UI message.
    std::string message;
};

/// Owned gardener data; independent of database and callback lifetimes.
struct Gardener {
    id_t id;
    std::string name;
};

/// Completed operation outcome, distinct from whether its job was accepted.
using GardenerResult = std::expected<Gardener, MainverteError>;

}  // namespace mv
