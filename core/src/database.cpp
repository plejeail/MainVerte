#include "database.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#include <sqlite3.h>

#include "sql_scripts.hpp"
#include "tools.h"


namespace {
static constexpr size_t DATABASE_VERSION = mv::sql::scripts.size();
static const auto TRANSIENT = SQLITE_TRANSIENT;  // NOLINT(performance-no-int-to-ptr)

const char* to_string(Database::State state) {
    switch (state) {
    case Database::State::Uninitialized: return "uninitialized";
    case Database::State::Initialized:   return "initialized";
    case Database::State::Running:       return "running";
    case Database::State::Stopping:      return "stopping";
    }

    assert(false);
    return "";
}

MV_NOINLINE std::string_view get_error(int code) {
    assert(code > 0 && code < 1'000'000);
    auto* data = sqlite3_errstr(code);
    assert(data != nullptr);
    return {data, strlen(data)};
}

MV_INLINE bool execute(sqlite3* handle, const char* sql) {
    assert(handle != nullptr);
    assert(sql != nullptr);
    auto res = sqlite3_exec(handle, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    return res;
}

MV_INLINE bool begin_transaction(sqlite3* handle) {
    return execute(handle, "BEGIN;");
}

MV_INLINE bool end_transaction(sqlite3* handle) {
    return execute(handle, "COMMIT;");
}

std::optional<size_t> get_database_version(sqlite3* handle) {
    assert(handle != nullptr);
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(handle, "PRAGMA user_version;", -1, &stmt, nullptr);
    assert(rc == SQLITE_OK);
    if (rc != SQLITE_OK) {
        log_err("DB: failed to prepare version request: {}", sqlite3_errmsg(handle));
        return std::nullopt;
    }

    rc = sqlite3_step(stmt);
    assert(rc == SQLITE_ROW);
    if (rc != SQLITE_ROW) {
        std::string error = sqlite3_errmsg(handle);
        sqlite3_finalize(stmt);
        log_err("DB: failed to evalute version request: {}", error);
        return std::nullopt;
    }

    int version = sqlite3_column_int(stmt, 0);
    assert(version >= 0);

    rc = sqlite3_finalize(stmt);
    assert(rc == SQLITE_OK);

    return {static_cast<size_t>(version)};
}

MV_INLINE bool set_database_version(sqlite3* handle, size_t version) {
    assert(version < 1'000'000);
    const std::string update_version = std::format("PRAGMA user_version = {};", version);
    return execute(handle, update_version.c_str());
}

MV_NOINLINE bool verify_pragma(sqlite3* handle, const char* query, std::string_view expected) {
    assert(handle != nullptr);
    assert(query != nullptr);
    assert(!expected.empty());

    sqlite3_stmt* raw = nullptr;
    const int prepared = sqlite3_prepare_v2(handle, query, -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, &sqlite3_finalize);
    if (prepared != SQLITE_OK) {
        log_err("DB: cannot prepare {}: {}", query, sqlite3_errmsg(handle));
        return false;
    }

    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        log_err("DB: cannot read {}: {}", query, sqlite3_errmsg(handle));
        return false;
    }

    const auto* text = sqlite3_column_text(statement.get(), 0);
    const std::string_view actual = text ? reinterpret_cast<const char*>(text) : "<null>";
    if (actual != expected) {
        log_err("DB: {} expected '{}', got '{}'", query, expected, actual);
        return false;
    }

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        log_err("DB: cannot finish {}: {}", query, sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_finalize(statement.release()) != SQLITE_OK) {
        log_err("DB: cannot finalize {}: {}", query, sqlite3_errmsg(handle));
        return false;
    }

    return true;
}

bool prepare_statement(sqlite3* handle, Database::statement_ptr& ptr, const char* query) {
    assert(handle != nullptr);
    assert(query != nullptr);

    if (ptr != nullptr) {
        return true;
    }

    sqlite3_stmt* statement;
    int rc = sqlite3_prepare_v2(handle, query, -1, &statement, nullptr);
    ptr.reset(statement);
    return rc == SQLITE_OK;
}
}  // namespace

bool detail::configure_database(sqlite3* handle) {
    const bool configured = execute(handle,
            "PRAGMA foreign_keys = ON;"
            "PRAGMA journal_mode = WAL;"
            "PRAGMA synchronous = NORMAL;"
            "PRAGMA cache_size = -8192;"
            "PRAGMA temp_store = MEMORY;"
            "PRAGMA busy_timeout = 3000;"
            "PRAGMA optimize = 0x10002;");
    if (!configured) {
        log_err("DB: cannot configure PRAGMA: {}", sqlite3_errmsg(handle));
        return false;
    }

    // In-memory databases cannot use WAL; SQLite keeps journal_mode=memory.
    const char* filename = sqlite3_db_filename(handle, "main");
    const char* journal_mode = filename && *filename ? "wal" : "memory";
    return verify_pragma(handle, "PRAGMA foreign_keys;", "1")
        && verify_pragma(handle, "PRAGMA journal_mode;", journal_mode)
        && verify_pragma(handle, "PRAGMA synchronous;", "1")
        && verify_pragma(handle, "PRAGMA cache_size;", "-8192")
        && verify_pragma(handle, "PRAGMA temp_store;", "2")
        && verify_pragma(handle, "PRAGMA busy_timeout;", "3000");
}

bool detail::migrate_script(sqlite3* handle, std::string_view script, size_t version) {
    assert(version < 1'000'000);
    if (!begin_transaction(handle)) {
        log_err("DB: cannot begin migration {}: {}", version, sqlite3_errmsg(handle));
        return false;
    }

    std::string sql(script);
    if (!execute(handle, sql.c_str()) || !set_database_version(handle, version) || !end_transaction(handle)) {
        log_err("DB: migration {} failed: {}", version, sqlite3_errmsg(handle));
        if (!sqlite3_get_autocommit(handle) && !execute(handle, "ROLLBACK;")) {
            log_err("DB: migration {} rollback failed: {}", version, sqlite3_errmsg(handle));
        }

        return false;
    }

    return true;
}

Database::Database()
: statement_create_gardener(nullptr, sqlite3_finalize)
, statement_read_settings(nullptr, sqlite3_finalize)
, statement_write_settings(nullptr, sqlite3_finalize)
{}

Database::~Database() {
    if (thread.joinable()) {
        {
            std::lock_guard lock(mutex);
            state = State::Stopping;
        }
        wake.notify_one();
        thread.join();
    }

    sqlite3_close_v2(handle);
}

void Database::start() {
    std::lock_guard lock(mutex);
    assert(handle != nullptr);
    assert(state == State::Initialized);

    if (state != State::Initialized) {
        log_err("DB::start invalid state {}", to_string(state));
        return;
    }

    state = State::Running;
    thread = std::thread([this]{ thread_loop(); });
}

bool Database::submit(Job job) {
    if (!job) {
        return false;
    }
    {
        std::lock_guard lock(mutex);
        if (state != State::Running) {
            return false;
        }
        assert(handle != nullptr);
        jobs.push_back(std::move(job));
    }
    wake.notify_one();
    return true;
}

bool Database::initialize(const std::string& path) {
    std::lock_guard lock(mutex);
    assert(handle == nullptr);
    assert(state == State::Uninitialized);

    if (state != State::Uninitialized) {
        log_dbg("DB: already initialized");
        return true;
    }

    MEASURE("DB::initialize");

    const auto directory = std::filesystem::path(std::u8string(path.begin(), path.end())).parent_path();
    if (!directory.empty()) {
        std::error_code error;
        const bool exists = std::filesystem::exists(directory, error);
        if (!error && !exists) {
            std::filesystem::create_directories(directory, error);
        }

        if (error) {
            log_err("DB: cannot prepare database directory '{}': {}", directory.string(), error.message());
            return false;
        }
    }

    int ec = sqlite3_initialize();
    if (ec != SQLITE_OK) {
        log_err("DB: sqlite3 initialization failed: {}", get_error(ec));
        return false;
    }

    ec = sqlite3_open_v2(path.c_str(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXRESCODE,
                         nullptr);
    if (ec != SQLITE_OK) {
        log_err("DB: cannot open database: {}", get_error(ec));
        sqlite3_close_v2(handle);
        handle = nullptr;
        return false;
    }

    if (!detail::configure_database(handle)) {
        log_err("DB: failed to configure database");
        sqlite3_close_v2(handle);
        handle = nullptr;
        return false;
    }

    auto current_version = get_database_version(handle);
    if (!current_version.has_value() || !migrate_database<DATABASE_VERSION>(*current_version)) {
        sqlite3_close_v2(handle);
        handle = nullptr;
        return false;
    }

    state = State::Initialized;
    return true;
}

template<size_t software_version>
bool Database::migrate_database(size_t current_version) {
    assert(handle != nullptr);

    if (current_version > 1'000'000) {
        log_err("DB: version is suspicously big");
        return false;
    }

    if (software_version < current_version) {
        log_err("DB: version downgrade not supported {} -> {}", current_version, software_version);
        return false;
    }

    bool success = true;
    for (size_t v = current_version; v < software_version; ++v) {

        if (!detail::migrate_script(handle, mv::sql::scripts[v], v + 1)) {
            return false;
        }
    }

    return success;
}

void Database::thread_loop() {
    std::unique_lock lock(mutex);
    assert(handle != nullptr);
    assert(state == State::Running || state == State::Stopping);

    for (;;) {
        wake.wait(lock, [this] { return state == State::Stopping || !jobs.empty(); });
        if (jobs.empty() && state == State::Stopping) {
            return;
        }

        auto job = std::move(jobs.front());
        jobs.pop_front();
        lock.unlock();
        job(handle);
        job = nullptr;  // Destroy captures outside the queue lock as well.
        lock.lock();
    }
}

int64_t Database::create_gardener(const std::string& name) {
    static constexpr const char* sql = "INSERT INTO gardener(display_name, created_at) VALUES (?1, unixepoch())";

    if (!prepare_statement(handle, statement_create_gardener, sql)) {
        // assert(false);
        return -1;
    }

    auto* statement = statement_create_gardener.get();
    auto str_size = static_cast<sqlite3_uint64>(name.size());
    int bind_rc = sqlite3_bind_text64(statement, 1, name.data(), str_size, TRANSIENT, SQLITE_UTF8);
    if (bind_rc != SQLITE_OK) {
        return -1;
    }

    int step_rc  = sqlite3_step(statement);
    int reset_rc = sqlite3_reset(statement);

    if (step_rc != SQLITE_DONE || reset_rc != SQLITE_OK) {
        return -1;
    }

    return sqlite3_last_insert_rowid(handle);
}

mv::GardenerResult Database::create_and_select_gardener(const std::string& name) {
    bool transaction_started = false;
    const auto fail = [this, &transaction_started](const char* stage) -> mv::GardenerResult {
        auto message = std::format("DB: cannot create and select gardener ({}): {}",
                                   stage, sqlite3_errmsg(handle));
        log_err(message);
        if (transaction_started && !sqlite3_get_autocommit(handle) && !execute(handle, "ROLLBACK;")) {
            const auto rollback_error = std::format("; rollback failed: {}", sqlite3_errmsg(handle));
            log_err(rollback_error);
            message += rollback_error;
        }
        return std::unexpected(mv::MainverteError{
            .code = mv::MainverteError::Code::Database, .message = std::move(message)});
    };

    if (!begin_transaction(handle)) {
        return fail("begin");
    }
    transaction_started = true;
    const auto id = create_gardener(name);
    if (id <= 0) {
        return fail("insert");
    }
    auto settings = read_settings();
    if (!settings) {
        return fail("read settings");
    }
    settings->current_gardener_id = id;
    settings->current_collection_id = 0;
    if (!write_settings(*settings)) {
        return fail("select");
    }
    if (!end_transaction(handle)) {
        return fail("commit");
    }
    return mv::Gardener{.id = id, .name = name};
}

std::optional<mv::Settings> Database::read_settings() {
    static constexpr const char* sql =
        "SELECT (SELECT id FROM gardener WHERE id = current_gardener_id AND id > 0), "
        "current_collection_id FROM settings WHERE id = 1";
    auto fail = [this]() -> std::optional<mv::Settings> {
        log_err("DB: cannot read settings (missing row, invalid ID or SQLite error): {}", sqlite3_errmsg(handle));
        if (statement_read_settings) {
            sqlite3_reset(statement_read_settings.get());
        }
        return std::nullopt;
    };
    if (!prepare_statement(handle, statement_read_settings, sql)) {
        return fail();
    }
    auto* statement = statement_read_settings.get();
    if (sqlite3_step(statement) != SQLITE_ROW) {
        return fail();
    }

    mv::Settings settings;
    std::array fields{&settings.current_gardener_id, &settings.current_collection_id};
    for (int column = 0; column < 2; ++column) {
        const int type = sqlite3_column_type(statement, column);
        if (type == SQLITE_NULL) {
            continue;
        }
        if (type != SQLITE_INTEGER) {
            return fail();
        }
        const auto value = sqlite3_column_int64(statement, column);
        *fields[column] = static_cast<mv::id_t>(value);
    }
    if (sqlite3_step(statement) != SQLITE_DONE) {
        return fail();
    }
    if (sqlite3_reset(statement) != SQLITE_OK) {
        return fail();
    }
    return settings;
}

bool Database::write_settings(const mv::Settings& settings) {
    static constexpr const char* sql =
        "UPDATE settings SET current_gardener_id = ?1, current_collection_id = ?2 WHERE id = 1";
    auto fail = [this] {
        log_err("DB: cannot write settings (missing row or SQLite error): {}", sqlite3_errmsg(handle));
        if (statement_write_settings) {
            sqlite3_reset(statement_write_settings.get());
        }
        return false;
    };
    if (!prepare_statement(handle, statement_write_settings, sql)) {
        return fail();
    }
    auto* statement = statement_write_settings.get();
    std::array fields{settings.current_gardener_id, settings.current_collection_id};
    for (int column = 0; column < 2; ++column) {
        const int rc = fields[column] == 0
            ? sqlite3_bind_null(statement, column + 1)
            : sqlite3_bind_int64(statement, column + 1, fields[column]);
        if (rc != SQLITE_OK) {
            return fail();
        }
    }
    // A single UPDATE keeps both fields atomic, including on constraint failures.
    if (sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(handle) != 1) {
        return fail();
    }
    if (sqlite3_reset(statement) != SQLITE_OK) {
        return fail();
    }
    return true;
}
