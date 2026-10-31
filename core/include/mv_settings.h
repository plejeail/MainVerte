// include/mv_settings.h ------------------------------------------------------
// MAINVERTE_CORE SETTINGS
//
// Define the core settings data structure.
//
//-------------------------------------------------------------- mainverte/core
#pragma once

#include <cstdint>
#include <string>


namespace mv {

/// Severity of a message emitted through platform::log().
enum class LogLevel : int8_t { Dbg, Inf, Wrn, Err, };

/// Owned snapshot returned by settings(); changing it does not update Core.
/// Submit the modified snapshot with save_settings() and observe completion
/// before assuming persistence succeeded. Access rules are documented in mv_core.h.
struct Settings {
    /// No selected object; stored as SQL NULL for selection fields.
    static constexpr int64_t Undefined = 0;

    bool        dark_theme{false};
    bool        debug_enabled{false};
    /// Minimum log severity.
    LogLevel    log_level{LogLevel::Inf};
    int64_t     current_gardener_id{Undefined};
    int64_t     current_collection_id{Undefined};
    /// Preferred language code (for example, "fr"); empty means system preference.
    /// The application resolves supported languages and fallback behavior.
    std::string language;
};

}  // namespace mv
