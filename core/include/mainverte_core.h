// include/core.h -------------------------------------------------------------
// MAINVERTE_CORE MAIN HEADER
//
// The consuming application implements mv::Platform functions and owns the
// mv::Services lifetime, calling initialize() before using Core services.
//
//-------------------------------------------------------------- mainverte/core
#pragma once

#include <cstdint>
#include <string_view>


namespace mv {

/// Severity of a message emitted through Platform::log().
enum class Level : int8_t { Dbg, Inf, Wrn, Err, };

/// Platform-specific facilities supplied by the application.
namespace Platform {
/// Sends a message to the application's logging facility.
///
/// The application provides the definition for each target platform. The
/// message is borrowed for the duration of this call and must not be retained.
/// Implementations should preserve the requested severity.
void log(Level severity, std::string_view msg);
};

/// Controls the lifetime of Core services on behalf of the application.
///
/// Service storage and initialization/destruction order are managed in core.cpp;
/// implementations remain private to Core and are accessed internally through
/// the service namespace. The application keeps this lifetime guard alive while
/// any consumer or pending operation can still use the services.
///
/// Use a single instance and call initialize() once, before using Core services.
/// These are caller requirements, not runtime-enforced guarantees. Initialize
/// and destroy the guard on the application owner thread, without concurrent
/// service access during initialization or teardown. Destroy consumers and
/// settle or cancel their pending operations before destroying this guard.
class Services {
public:
    Services() = default;
    /// Releases Core services, including resources retained after failed initialization.
    ~Services();

    Services(const Services&)            = delete;
    Services& operator=(const Services&) = delete;
    Services(Services&&)                 = delete;
    Services& operator=(Services&&)      = delete;

    /// Initializes Core services using a database path supplied by the application.
    /// The path is consumed during this call; its storage need not outlive it.
    /// Returns false on failure; do not use Core services in that case.
    /// Destruction still releases any partially initialized services.
    bool initialize(std::string_view database_path);
};
}
