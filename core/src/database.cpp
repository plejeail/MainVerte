#include <sqlite3.h>

#include "database.h"
#include "sql_scripts.hpp"
#include "tools.h"


namespace {
static constexpr ssize_t DATABASE_VERSION = mv::sql::scripts.size();
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

MV_NOINLINE std::string_view get_error(int error_code) {
    assert(error_code > 0);
    auto* data = sqlite3_errstr(error_code);
    return {data, strlen(data)};
}

MV_INLINE bool execute(sqlite3* handle, const char* sql) {
    assert(handle != nullptr);
    assert(sql != nullptr);

    auto res = sqlite3_exec(handle, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    assert(res);
    return res;
}

MV_INLINE bool begin_transaction(sqlite3* handle) {
    return execute(handle, "BEGIN;");
}

MV_INLINE bool end_transaction(sqlite3* handle) {
    return execute(handle, "COMMIT;");
}

std::optional<ssize_t> get_database_version(sqlite3* handle) {
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

    return {static_cast<ssize_t>(version)};
}

MV_INLINE bool set_database_version(sqlite3* handle, ssize_t version) {
    const std::string update_version = std::format("PRAGMA user_version = {};", version);
    return execute(handle, update_version.c_str());
}

MV_NOINLINE bool verify_pragma(sqlite3* handle, const char* query, std::string_view expected) {
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
    if (ptr != nullptr) {
        return sqlite3_reset(ptr.get()) == SQLITE_OK;
    }

    sqlite3_stmt* statement;
    int rc = sqlite3_prepare_v2(handle, query, -1, &statement, nullptr);
    ptr.reset(statement);
    return rc == SQLITE_OK;
}
}  // namespace

bool detail::configure_database(sqlite3* handle) {
    const int configured = sqlite3_exec(handle,
        "PRAGMA foreign_keys = ON;"
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA cache_size = -8192;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA busy_timeout = 3000;"
        "PRAGMA optimize = 0x10002;", nullptr, nullptr, nullptr);
    if (configured != SQLITE_OK) {
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

bool detail::migrate_script(sqlite3* handle, std::string_view script, ssize_t version) {
    assert(version >= 0);

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

template<ssize_t software_version>
bool Database::migrate_database(ssize_t current_version) {
    assert(handle != nullptr);

    if (current_version < 0) {
        log_err("DB: version is negative");
        return false;
    }

    if (software_version < current_version) {
        log_err("DB: version downgrade not supported {} -> {}", current_version, software_version);
        return false;
    }

    bool success = true;
    for (ssize_t v = current_version; v < software_version; ++v) {

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
    int rc = sqlite3_bind_text64(statement, 1, name.data(), str_size, TRANSIENT, SQLITE_UTF8);
    if (rc != SQLITE_OK) {
        return -1;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE) {
        return -1;
    }

    return sqlite3_last_insert_rowid(handle);
}
