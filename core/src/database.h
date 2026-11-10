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
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <variant>

#include "mv_definitions.h"
#include "mv_move_only_function.h"
#include "mv_settings.h"


struct sqlite3;
struct sqlite3_stmt;

class Query {
public:
    using query_variant_t = std::variant<std::monostate, int32_t, int64_t, double, std::string_view>;

    Query() = default;
    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;
    ~Query();

    // Statements are single-use locals: finalized on destruction, so a refused or failed
    // operation cannot leave bindings or a half-stepped statement behind. Preparing again
    // replaces the previous statement.
    bool prepare(sqlite3* handle, const char* sql);

    bool bind(int32_t pos, std::string_view value);
    bool bind(int32_t pos, double value);
    bool bind(int32_t pos, int64_t value);
    bool bind(int32_t pos, bool value);
    bool bind_null(int32_t pos);

    int step();

    // Typed access to the current row. std::nullopt means SQL NULL or a value that T cannot
    // represent exactly: there is no implicit SQLite conversion (REAL truncated to an integer,
    // TEXT read as 0) and no narrowing into an enum's underlying type. For a scoped enum only
    // representability is checked; callers validate the enumerators they accept.
    template <typename T>
    [[nodiscard]] std::optional<T> column(int index) const;
    template <typename T> requires std::is_scoped_enum_v<T>
    [[nodiscard]] std::optional<T> column(int index) const;

private:
    sqlite3_stmt* _statement = nullptr;
};

template <> std::optional<int64_t> Query::column<int64_t>(int index) const;
template <> std::optional<bool> Query::column<bool>(int index) const;
template <> std::optional<std::string_view> Query::column<std::string_view>(int index) const;

class Database {
public:
    using Job = mv::MoveOnlyFunction<void(sqlite3*)>;

    enum class State : int8_t { Uninitialized, Initialized, Running, Stopping, };
    enum class Error : int8_t { NotRunning };

    ~Database();

    bool initialize(const std::string& path);

    void start();

    // Internal persistence jobs run serially on the worker. Never retain the handle.
    // Jobs must not throw; they deliver their own results on the worker thread.
    bool submit(Job job);

    int64_t create_gardener(const std::string& name);
    mv::GardenerResult complete_setup(const std::string& name, const std::string& language,
                                      const std::string& initial_collection_name = {});

    // Use before start() on the owner thread, or on the worker after start().
    std::optional<mv::Settings> read_settings();
    bool write_settings(const mv::Settings& settings);

    mv::CareResult update_care(mv::id_t gardener, const mv::CareAction& action);
    mv::PlantsResult update_plants(mv::id_t gardener, const mv::PlantAction& a, const std::string& initial);
private:
    void thread_loop();

    template <size_t software_version> bool migrate_database(size_t current_version);

    State           _state = State::Uninitialized;
    std::mutex      _mutex;
    std::condition_variable _wake;
    sqlite3*        _handle = nullptr;
    std::deque<Job> _jobs;
    std::thread     _thread;
};

namespace detail {

bool configure_database(sqlite3* handle);
bool migrate_script(sqlite3* handle, std::string_view script, size_t version);

}  // namespace detail
