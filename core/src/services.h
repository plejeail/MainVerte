// src/services.h -------------------------------------------------------------
// GLOBAL SERVICES
//
// A service is an object accessible throughout Core, with a lifetime spanning
// most of the application's lifetime. The consuming application must initialize
// services before use and control their lifetime through mv::Services.
//
// ------------------------------------------------------------- mainverte/core
#pragma once

class Database;


// Services are initialized and destroyed exclusively in core.cpp.
// The application controls their lifetime through mv::Services,
// declared in mainverte_core.h.
//
// Internal consumers borrow these services; they must not reset or move them.
namespace service {
extern std::unique_ptr<Database> database;
}  // service
