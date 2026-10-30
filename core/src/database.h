// src/database.h -------------------------------------------------------------
// DATABASE MANAGEMENT
//
// Internal Core component owning a SQLite connection and a worker thread.
// Initialize synchronously with an application-supplied path, then call start().
// The worker executes queued jobs in FIFO order and sleeps when the queue is
// empty. Producers may submit concurrently; SQLite access stays on the worker.
// Jobs must not throw or retain the connection handle.
//
// submit(job) reports acceptance, not completion. Jobs deliver their own results
// on the worker thread; captured data must remain valid until execution finishes.
//
// Initialize, start and destroy on the owner thread. Stop producers before
// destruction, which drains accepted jobs, joins the worker and closes SQLite.
//
// SQLite is compiled with SQLITE_THREADSAFE=0. This application uses one
// Database instance; no other connection may access SQLite concurrently.
//
// ------------------------------------------------------------- mainverte/core
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "settings.h"
#include "definitions.h"
#include "move_only_function.h"

struct sqlite3;
struct sqlite3_stmt;


class Database {
public:
    using statement_ptr = std::unique_ptr<sqlite3_stmt, int(*)(sqlite3_stmt*)>;

    using Job = mv::MoveOnlyFunction<void(sqlite3*)>;

    enum class State : int8_t { Uninitialized, Initialized, Running, Stopping, };
    enum class Error : int8_t { NotRunning };

    Database();
    ~Database();

    bool initialize(const std::string& path);

    void start();

    // Internal persistence jobs run serially on the worker. Never retain the handle.
    // Jobs must not throw; they deliver their own results on the worker thread.
    bool submit(Job job);

    int64_t create_gardener(const std::string& name);
    mv::GardenerResult create_and_select_gardener(const std::string& name);

    // Use before start() on the owner thread, or on the worker after start().
    std::optional<mv::Settings> read_settings();
    bool write_settings(const mv::Settings& settings);

private:
    void thread_loop();

    template <size_t software_version> bool migrate_database(size_t current_version);

    State                   _state = State::Uninitialized;
    std::mutex              _mutex;
    std::condition_variable _wake;
    sqlite3*                _handle = nullptr;
    std::deque<Job>         _jobs;
    std::thread             _thread;
    statement_ptr           _statement_create_gardener;
    statement_ptr           _statement_read_settings;
    statement_ptr           _statement_write_settings;
};

namespace detail {
bool configure_database(sqlite3* handle);
bool migrate_script(sqlite3* handle, std::string_view script, size_t version);
}
