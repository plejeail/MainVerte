// src/services.h -------------------------------------------------------------
// GLOBAL SERVICES
//
// A service is an object accessible throughout Core, with a lifetime spanning
// most of the application's lifetime. The consuming application must initialize
// services before use and control their lifetime through mv::Services.
//
// ------------------------------------------------------------- mainverte/core
#pragma once

class  Database;
struct Settings;


// Services are initialized and destroyed exclusively in services.cpp.
// The consummer of core controls their lifetime through mv::Services,
// declared in mainverte_core.h.
// Services shall not be used before the consummer call to Services::initialize
// or after the destruction of the consummer instance of Services.
//
// Internal consumers borrow these services; they must not reset or move them.
namespace service {
Settings& settings();
Database& database();
}  // service
