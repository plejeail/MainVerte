#include "database.h"

#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include <sqlite3.h>

#include "mv_definitions.h"
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

MV_INLINE bool transaction_begin(sqlite3* handle) {
    return execute(handle, "BEGIN;");
}

MV_INLINE bool transaction_begin_immediate(sqlite3* handle) {
    return execute(handle, "BEGIN IMMEDIATE;");
}

MV_INLINE bool transaction_end(sqlite3* handle) {
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

// The supported range of NeedType is deliberately repeated in the CHECK constraint of
// core/sql/0001_specimen_care.sql, which cannot change once shipped. Each copy fails closed;
// the "care need type range agrees" test keeps them aligned.
bool valid_need_type(mv::NeedType type) {
    return type >= mv::NeedType::Watering && type <= mv::NeedType::Temperature;
}

bool valid_name(const std::string& name) {
    return name.find('\0') == std::string::npos && name.find_first_not_of(" \t\r\n") != std::string::npos;
}

bool valid_date(const std::optional<std::string>& value) {
    if (!value) {
        return true;
    }

    const auto& date = *value;
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }

    int year = 0, month = 0, day = 0;
    const auto parse = [&](size_t offset, size_t length, int& result) {
        const auto* begin = date.data() + offset;
        const auto [end, error] = std::from_chars(begin, begin + length, result);
        return error == std::errc{} && end == begin + length && date[offset] >= '0' && date[offset] <= '9';
    };

    return parse(0, 4, year) && parse(5, 2, month) && parse(8, 2, day) && year > 0 &&
        std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                   std::chrono::day{static_cast<unsigned>(day)}}.ok();
}

}  // namespace

Query::~Query() {
    sqlite3_finalize(_statement);  // already check for nullptr
}

bool Query::prepare(sqlite3* handle, const char* sql) {
    assert(handle != nullptr);
    assert(sql != nullptr);

    sqlite3_finalize(_statement);  // Replaces any previous statement; finalizing nullptr is a no-op.
    _statement = nullptr;
    return sqlite3_prepare_v2(handle, sql, -1, &_statement, nullptr) == SQLITE_OK;
}

bool Query::bind(int32_t pos, std::string_view value) {
    auto res = sqlite3_bind_text64(_statement, pos, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
    return res == SQLITE_OK;
}

bool Query::bind(int32_t pos, double value) {
    return sqlite3_bind_double(_statement, pos, value) == SQLITE_OK;
}

bool Query::bind(int32_t pos, int64_t value) {
    return sqlite3_bind_int64(_statement, pos, value) == SQLITE_OK;
}

MV_INLINE bool Query::bind(int32_t pos, bool value) {
    return bind(pos, static_cast<int64_t>(value ? 1 : 0));
}

bool Query::bind_null(int32_t pos) {
    return sqlite3_bind_null(_statement, pos) == SQLITE_OK;
}

int Query::step() {
    return sqlite3_step(_statement);
}

template <>
std::optional<int64_t> Query::column<int64_t>(int index) const {
    // sqlite3_column_int64 converts any storage class: 1.5 reads as 1 and 'text' as 0.
    if (sqlite3_column_type(_statement, index) != SQLITE_INTEGER) {
        return std::nullopt;
    }

    return static_cast<int64_t>(sqlite3_column_int64(_statement, index));
}

template <typename T> requires std::is_scoped_enum_v<T>
MV_INLINE std::optional<T> Query::column(int index) const {
    using Underlying = std::underlying_type_t<T>;

    // static_cast would silently wrap an out-of-range value into the underlying type.
    const auto value = column<int64_t>(index);
    if (!value || *value < std::numeric_limits<Underlying>::min() || *value > std::numeric_limits<Underlying>::max()) {
        return std::nullopt;
    }

    return static_cast<T>(*value);
}

template <>
std::optional<bool> Query::column<bool>(int index) const {
    const auto value = column<int64_t>(index);
    if (!value || (*value != 0 && *value != 1)) {
        return std::nullopt;
    }

    return *value == 1;
}

template <>
std::optional<std::string_view> Query::column<std::string_view>(int index) const {
    if (sqlite3_column_type(_statement, index) == SQLITE_NULL) {
        return std::nullopt;
    }

    return std::string_view(
        reinterpret_cast<const char*>(sqlite3_column_text(_statement, index)),
        sqlite3_column_bytes(_statement, index)
    );
}

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
    if (!transaction_begin(handle)) {
        log_err("DB: cannot begin migration {}: {}", version, sqlite3_errmsg(handle));
        return false;
    }

    std::string sql(script);
    if (!execute(handle, sql.c_str()) || !set_database_version(handle, version) || !transaction_end(handle)) {
        log_err("DB: migration {} failed: {}", version, sqlite3_errmsg(handle));
        if (!sqlite3_get_autocommit(handle) && !execute(handle, "ROLLBACK;")) {
            log_err("DB: migration {} rollback failed: {}", version, sqlite3_errmsg(handle));
        }

        return false;
    }

    return true;
}

Database::~Database() {
    if (_thread.joinable()) {
        {
            std::lock_guard lock(_mutex);
            _state = State::Stopping;
        }
        _wake.notify_one();
        _thread.join();
    }

    sqlite3_close_v2(_handle);
}

void Database::start() {
    std::lock_guard lock(_mutex);
    assert(_handle != nullptr);
    assert(_state == State::Initialized);

    if (_state != State::Initialized) {
        log_err("DB start invalid state {}", to_string(_state));
        return;
    }

    _state = State::Running;
    _thread = std::thread([this]{ thread_loop(); });
}

bool Database::submit(Job job) {
    if (!job) {
        return false;
    }

    {
        std::lock_guard lock(_mutex);
        if (_state != State::Running) {
            return false;
        }

        assert(_handle != nullptr);
        _jobs.push_back(std::move(job));
    }
    _wake.notify_one();
    return true;
}

bool Database::initialize(const std::string& path) {
    std::lock_guard lock(_mutex);
    assert(_handle == nullptr);
    assert(_state == State::Uninitialized);

    if (_state != State::Uninitialized) {
        log_dbg("DB: already initialized");
        return true;
    }

    MEASURE("DB initialize");

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

    ec = sqlite3_open_v2(path.c_str(), &_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXRESCODE,
                         nullptr);
    if (ec != SQLITE_OK) {
        log_err("DB: cannot open database: {}", get_error(ec));
        sqlite3_close_v2(_handle);
        _handle = nullptr;
        return false;
    }

    if (!detail::configure_database(_handle)) {
        log_err("DB: failed to configure database");
        sqlite3_close_v2(_handle);
        _handle = nullptr;
        return false;
    }

    auto current_version = get_database_version(_handle);
    if (!current_version.has_value() || !migrate_database<DATABASE_VERSION>(*current_version)) {
        sqlite3_close_v2(_handle);
        _handle = nullptr;
        return false;
    }

    _state = State::Initialized;
    return true;
}

template<size_t software_version>
bool Database::migrate_database(size_t current_version) {
    assert(_handle != nullptr);

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
        if (!detail::migrate_script(_handle, mv::sql::scripts[v], v + 1)) {
            return false;
        }
    }

    return success;
}

void Database::thread_loop() {
    std::unique_lock lock(_mutex);
    assert(_handle != nullptr);
    assert(_state == State::Running || _state == State::Stopping);

    for (;;) {
        _wake.wait(lock, [this] { return _state == State::Stopping || !_jobs.empty(); });
        if (_jobs.empty() && _state == State::Stopping) {
            return;
        }

        auto job = std::move(_jobs.front());
        _jobs.pop_front();
        lock.unlock();
        job(_handle);
        job = nullptr;  // Destroy captures outside the queue lock as well.
        lock.lock();
    }
}

int64_t Database::create_gardener(const std::string& name) {
    Query insert;
    static constexpr const char* sql = "INSERT INTO gardener(display_name, created_at) VALUES (?1, unixepoch())";
    if (!insert.prepare(_handle, sql) || !insert.bind(1, name) || insert.step() != SQLITE_DONE) {
        return -1;
    }

    return sqlite3_last_insert_rowid(_handle);
}

mv::GardenerResult Database::complete_setup(const std::string& name, const std::string& language, const std::string& initial_collection_name) {
    bool transaction_started = false;
    const auto fail = [this, &transaction_started](const char* stage) -> mv::GardenerResult {
        auto message = std::format("DB: cannot complete setup ({}): {}",
                                   stage, sqlite3_errmsg(_handle));
        log_err(message);
        if (transaction_started && !sqlite3_get_autocommit(_handle) && !execute(_handle, "ROLLBACK;")) {
            const auto rollback_error = std::format("; rollback failed: {}", sqlite3_errmsg(_handle));
            log_err(rollback_error);
            message += rollback_error;
        }

        return std::unexpected(mv::MainverteError{
            .code = mv::MainverteError::Code::Database, .message = std::move(message)});
    };

    if (!transaction_begin(_handle)) {
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
    settings->language = language;
    if (!initial_collection_name.empty()) {
        Query collection;
        static constexpr const char* sql =
            "INSERT INTO collection(gardener_id,name,created_at,modified_at) VALUES(?1,?2,unixepoch(),unixepoch())";
        if (!collection.prepare(_handle, sql) || !collection.bind(1, id)
            || !collection.bind(2, std::string_view(initial_collection_name)) || collection.step() != SQLITE_DONE) {
            return fail("initial collection");
        }

        settings->current_collection_id = sqlite3_last_insert_rowid(_handle);
    }

    if (!write_settings(*settings)) {
        return fail("write settings");
    }

    if (!transaction_end(_handle)) {
        return fail("commit");
    }

    return mv::Gardener{.id = id, .name = name};
}

std::optional<mv::Settings> Database::read_settings() {
    // Selections are resolved while reading: a stored gardener or collection that does not
    // exist reads as "no selection". Ownership of the collection is enforced when writing and
    // repaired by update_plants, and a collection may be selected without a gardener.
    static constexpr const char* sql =
        "SELECT g.id,"
               "c.id,"
               "s.language,"
               "s.dark_theme,"
               "s.log_level,"
               "s.debug_enabled"
        " FROM settings s"
        " LEFT JOIN gardener g ON g.id = s.current_gardener_id"
        " LEFT JOIN collection c ON c.id = s.current_collection_id"
        " WHERE s.id = 1";

    // The statement is local and finalized before returning. Error details are captured while
    // it is alive: finalizing it can replace the connection's message.
    std::string error;
    std::optional<mv::Settings> settings;
    {
        Query select;
        int rc = SQLITE_ERROR;
        if (!select.prepare(_handle, sql) || (rc = select.step()) != SQLITE_ROW) {
            error = rc == SQLITE_DONE ? "settings row is missing" : sqlite3_errmsg(_handle);
        } else {
            const auto language = select.column<std::string_view>(2);
            const auto dark_theme = select.column<bool>(3);
            const auto log_level = select.column<int64_t>(4);
            const auto debug_enabled = select.column<bool>(5);

            if (!language || !dark_theme || !debug_enabled) {
                error = "invalid stored language or presentation flag";
            } else if (!log_level || *log_level < static_cast<int64_t>(mv::LogLevel::Dbg)
                                  || *log_level > static_cast<int64_t>(mv::LogLevel::Err)) {
                error = "invalid stored log level";
            } else {
                // Copy everything out of the current row before stepping again: once the
                // statement reports SQLITE_DONE its columns no longer refer to a row.
                mv::Settings decoded;
                // Selections are nullable: NULL reads as Settings::Undefined.
                decoded.current_gardener_id = select.column<int64_t>(0).value_or(mv::Settings::Undefined);
                decoded.current_collection_id = select.column<int64_t>(1).value_or(mv::Settings::Undefined);
                decoded.language = *language;
                decoded.dark_theme_enabled = *dark_theme;
                decoded.log_level = static_cast<mv::LogLevel>(*log_level);
                decoded.debug_enabled = *debug_enabled;

                if (const int next = select.step(); next != SQLITE_DONE) {
                    error = next == SQLITE_ROW ? "unexpected extra settings row" : sqlite3_errmsg(_handle);
                } else {
                    settings = std::move(decoded);
                }
            }
        }
    }

    if (!settings) {
        log_err("DB: cannot read settings: {}", error);
    }

    return settings;
}

bool Database::write_settings(const mv::Settings& settings) {
    std::string error;
    if (settings.current_gardener_id > 0 && settings.current_collection_id > 0) {
        int owned = SQLITE_ERROR;
        {
            Query owner;
            static constexpr const char* sql = "SELECT 1 FROM collection WHERE id=?1 AND gardener_id=?2";
            if (owner.prepare(_handle, sql) && owner.bind(1, settings.current_collection_id)
                && owner.bind(2, settings.current_gardener_id)) {
                owned = owner.step();
            }

            if (owned != SQLITE_ROW && owned != SQLITE_DONE) {
                error = sqlite3_errmsg(_handle);
            }
        }

        if (!error.empty()) {
            log_err("DB: cannot verify collection selection: {}", error);
            return false;
        }

        if (owned == SQLITE_DONE) {
            log_wrn("DB: collection selection does not belong to gardener");
            return false;
        }
    }

    {
        Query update;
        static constexpr const char* sql = "UPDATE settings"
                                           " SET current_gardener_id = ?1,"
                                               "current_collection_id = ?2,"
                                               "language = ?3,"
                                               "dark_theme = ?4,"
                                               "log_level = ?5,"
                                               "debug_enabled = ?6"
                                           " WHERE id = 1";
        const bool bound = update.prepare(_handle, sql)
            && (settings.current_gardener_id == mv::Settings::Undefined
                    ? update.bind_null(1) : update.bind(1, settings.current_gardener_id))
            && (settings.current_collection_id == mv::Settings::Undefined
                    ? update.bind_null(2) : update.bind(2, settings.current_collection_id))
            && update.bind(3, settings.language)
            && update.bind(4, settings.dark_theme_enabled)
            && update.bind(5, static_cast<int64_t>(settings.log_level))
            && update.bind(6, settings.debug_enabled);
        if (!bound || update.step() != SQLITE_DONE) {
            error = sqlite3_errmsg(_handle);
        } else if (sqlite3_changes(_handle) != 1) {
            error = "settings row is missing";
        }
    }

    if (!error.empty()) {
        log_err("DB: cannot write settings: {}", error);
        return false;
    }

    return true;
}

mv::CareResult Database::update_care(mv::id_t gardener, const mv::CareAction& action) {
    using K = mv::CareAction::Kind;
    const auto fail = [this](std::string message, mv::MainverteError::Code code = mv::MainverteError::Code::Database) -> mv::CareResult {
        if (!sqlite3_get_autocommit(_handle) && sqlite3_exec(_handle, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
            message += "; rollback failed: ";
            message += sqlite3_errmsg(_handle);
        }

        mv::platform::log(mv::LogLevel::Wrn, message);
        return std::unexpected(mv::MainverteError{code, std::move(message)});
    };

    if (action.kind < K::List
        || action.kind > K::Delete
        || (action.kind == K::Add && !valid_need_type(action.type))) {
        return fail("Unsupported care operation or need type", mv::MainverteError::Code::InvalidInput);
    }

    if (!transaction_begin_immediate(_handle)) {
        return fail(sqlite3_errmsg(_handle));
    }

    // Every statement is a local Query, finalized before the transaction ends. A refused
    // or failed operation therefore leaves no binding or step state for later operations.
    // Error details are captured inside each block, while the failing statement is still
    // alive: finalizing it or rolling back can replace the connection's message.
    std::string error;
    int owned = SQLITE_ERROR;
    {
        Query owner;
        static constexpr const char* sql = "SELECT 1 FROM specimen s"
                                           " JOIN collection c ON c.id=s.collection_id"
                                           " WHERE s.id=?1 AND c.gardener_id=?2";
        if (!owner.prepare(_handle, sql) || !owner.bind(1, action.specimen_id) || !owner.bind(2, gardener)) {
            error = sqlite3_errmsg(_handle);
        } else {
            owned = owner.step();
            if (owned != SQLITE_ROW && owned != SQLITE_DONE) {
                error = sqlite3_errmsg(_handle);
            }
        }
    }

    if (!error.empty()) {
        return fail(std::move(error));
    }

    if (owned == SQLITE_DONE) {
        return fail("Specimen unavailable", mv::MainverteError::Code::NotFound);
    }

    if (action.kind == K::Add) {
        {
            Query insert;
            static constexpr const char* sql = "INSERT INTO care_need(specimen_id,type) VALUES(?,?)";
            if (!insert.prepare(_handle, sql) || !insert.bind(1, action.specimen_id)
                || !insert.bind(2, static_cast<int64_t>(action.type))) {
                error = sqlite3_errmsg(_handle);
            } else if (insert.step() != SQLITE_DONE) {
                error = sqlite3_errmsg(_handle);
            }
        }

        if (!error.empty()) {
            return fail(std::move(error));
        }
    } else if (action.kind == K::Delete) {
        int changes = 0;
        {
            Query remove;
            static constexpr const char* sql = "DELETE FROM care_need WHERE id=? AND specimen_id=?";
            if (!remove.prepare(_handle, sql) || !remove.bind(1, action.id) || !remove.bind(2, action.specimen_id)) {
                error = sqlite3_errmsg(_handle);
            } else if (remove.step() != SQLITE_DONE) {
                error = sqlite3_errmsg(_handle);
            } else {
                changes = sqlite3_changes(_handle);
            }
        }

        if (!error.empty()) {
            return fail(std::move(error));
        }

        if (changes != 1) {
            return fail("Care need unavailable", mv::MainverteError::Code::NotFound);
        }
    }

    std::vector<mv::CareNeed> result;
    {
        Query select;
        static constexpr const char* sql = "SELECT id,specimen_id,type"
                                           " FROM care_need"
                                           " WHERE specimen_id=?1 AND (?2=0 OR id=?2)"
                                           " ORDER BY id";
        if (!select.prepare(_handle, sql) || !select.bind(1, action.specimen_id)
            || !select.bind(2, action.kind == K::Get ? action.id : 0)) {
            error = sqlite3_errmsg(_handle);
        } else {
            int rc;
            while ((rc = select.step()) == SQLITE_ROW) {
                const auto id = select.column<int64_t>(0);
                const auto specimen = select.column<int64_t>(1);
                const auto type = select.column<mv::NeedType>(2);
                if (!id || !specimen || !type || !valid_need_type(*type)) {
                    error = "Stored care need is invalid";
                    break;
                }

                result.emplace_back(*id, *specimen, *type);
            }

            if (error.empty() && rc != SQLITE_DONE) {
                error = sqlite3_errmsg(_handle);
            }
        }
    }

    if (!error.empty()) {
        return fail(std::move(error));
    }

    if (action.kind == K::Get && (action.id <= 0 || result.empty())) {
        return fail("Care need unavailable", mv::MainverteError::Code::NotFound);
    }

    if (!transaction_end(_handle)) {
        return fail(sqlite3_errmsg(_handle));
    }

    return result;
}

mv::PlantsResult Database::update_plants(mv::id_t gardener, const mv::PlantAction& a, const std::string& initial) {
    using K = mv::PlantAction::Kind;
    const auto fail = [&](std::string message, mv::MainverteError::Code code = mv::MainverteError::Code::Database) -> mv::PlantsResult {
        if (!sqlite3_get_autocommit(_handle) && sqlite3_exec(_handle, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
            message += "; rollback failed: ";
            message += sqlite3_errmsg(_handle);
        }

        mv::platform::log(mv::LogLevel::Wrn, message);
        return std::unexpected(mv::MainverteError{code, std::move(message)});
    };
    if (gardener <= 0 || !transaction_begin_immediate(_handle)) {
        return fail("Unable to open plant operation");
    }

    Query owner;
    if (!owner.prepare(_handle, "SELECT id FROM gardener WHERE id=?")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!owner.bind(1, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (owner.step() != SQLITE_ROW) {
        return fail("Gardener does not exist");
    }

    Query count;
    if (!count.prepare(_handle, "SELECT id FROM collection WHERE gardener_id=? ORDER BY id LIMIT 1")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!count.bind(1, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    const int count_rc = count.step();
    if (count_rc == SQLITE_DONE && a.kind != K::CreateCollection) {
        Query insert;
        if (!insert.prepare(_handle, "INSERT INTO collection(gardener_id,name,created_at,modified_at) VALUES(?,?,unixepoch(),unixepoch())")) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!insert.bind(1, gardener)) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!insert.bind(2, initial)) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!valid_name(initial) || insert.step() != SQLITE_DONE) {
            return fail("Unable to create initial collection");
        }
    } else if (count_rc != SQLITE_ROW && count_rc != SQLITE_DONE) {
        return fail(sqlite3_errmsg(_handle));
    }

    Query repair;
    if (!repair.prepare(_handle, "UPDATE settings SET current_collection_id=(SELECT id FROM collection WHERE gardener_id=? ORDER BY id LIMIT 1) WHERE id=1 AND current_gardener_id=? AND NOT EXISTS(SELECT 1 FROM collection WHERE id=current_collection_id AND gardener_id=?)")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!repair.bind(1, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!repair.bind(2, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!repair.bind(3, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (repair.step() != SQLITE_DONE) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (a.kind == K::DeleteCollection) {
        Query last;
        if (!last.prepare(_handle, "SELECT 1 FROM collection WHERE id=?1 AND gardener_id=?2 AND (SELECT count(*) FROM collection WHERE gardener_id=?2)=1")) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!last.bind(1, a.id)) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!last.bind(2, gardener)) {
            return fail(sqlite3_errmsg(_handle));
        }

        const int rc = last.step();
        if (rc == SQLITE_ROW) {
            return fail("A gardener must retain at least one collection", mv::MainverteError::Code::LastCollection);
        }

        if (rc != SQLITE_DONE) {
            return fail(sqlite3_errmsg(_handle));
        }
    }

    const char* sql = nullptr;
    switch (a.kind) {
    case K::Browse:
    case K::GetSpecimen:
    case K::ListSpecimens: break;
    case K::CreateCollection: sql = "INSERT INTO collection(name,gardener_id,created_at,modified_at) VALUES(?1,?2,unixepoch(),unixepoch())"; break;
    case K::RenameCollection: sql = "UPDATE collection SET name=?1,modified_at=unixepoch() WHERE gardener_id=?2 AND id=?3"; break;
    case K::SelectCollection: sql = "UPDATE settings SET current_collection_id=?3 WHERE id=1 AND current_gardener_id=?2 AND EXISTS(SELECT 1 FROM collection WHERE id=?3 AND gardener_id=?2)"; break;
    case K::DeleteCollection: sql = "DELETE FROM collection WHERE gardener_id=?2 AND id=?3 AND NOT EXISTS(SELECT 1 FROM specimen WHERE collection_id=?3)"; break;
    case K::CreateSpecimen: sql = "INSERT INTO specimen(display_name,collection_id,botanical_name,cultivar,profile,acquired_on,created_at,modified_at) SELECT ?1,id,?4,?5,?7,?8,unixepoch(),unixepoch() FROM collection WHERE gardener_id=?2 AND id=?3"; break;
    case K::UpdateSpecimen: sql = "UPDATE specimen SET display_name=?1,botanical_name=?4,cultivar=?5,profile=CASE WHEN ?9 OR ?7 IS NOT NULL THEN ?7 ELSE profile END,acquired_on=CASE WHEN ?9 OR ?8 IS NOT NULL THEN ?8 ELSE acquired_on END,modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2)"; break;
    case K::MoveSpecimen: sql = "UPDATE specimen SET collection_id=?6,modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2) AND EXISTS(SELECT 1 FROM collection WHERE id=?6 AND gardener_id=?2)"; break;
    case K::ArchiveSpecimen: sql = "UPDATE specimen SET archived_at=unixepoch(),modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2)"; break;
    case K::RestoreSpecimen: sql = "UPDATE specimen SET archived_at=NULL,modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2)"; break;
    }

    if (sql) {
        if ((a.kind == K::CreateCollection || a.kind == K::RenameCollection || a.kind == K::CreateSpecimen || a.kind == K::UpdateSpecimen) && !valid_name(a.name)) {
            return fail("A name is required", mv::MainverteError::Code::InvalidInput);
        }

        if ((a.kind == K::CreateSpecimen || a.kind == K::UpdateSpecimen) &&
            (!valid_date(a.acquired_on) || (a.profile && (*a.profile < mv::PlantProfile::Succulent || *a.profile > mv::PlantProfile::Other)))) {
            return fail("Invalid profile or acquisition date", mv::MainverteError::Code::InvalidInput);
        }

        Query write;
        if (!write.prepare(_handle, sql)) {
            return fail(sqlite3_errmsg(_handle));
        }

        // Bind only the positions used by this operation.
        bool bound = write.bind(2, gardener);
        if (a.kind == K::CreateCollection || a.kind == K::RenameCollection ||
            a.kind == K::CreateSpecimen || a.kind == K::UpdateSpecimen) {
            bound = bound && write.bind(1, a.name);
        }

        if (a.kind != K::CreateCollection) {
            bound = bound && write.bind(3, a.id);
        }

        if (a.kind == K::MoveSpecimen) {
            bound = bound && write.bind(6, a.destination);
        }

        if (a.kind == K::CreateSpecimen || a.kind == K::UpdateSpecimen) {
            bound = bound && write.bind(4, a.botanical_name) && write.bind(5, a.cultivar);
            bound = bound && (a.profile ? write.bind(7, static_cast<int64_t>(*a.profile)) : write.bind_null(7));
            bound = bound && (a.acquired_on ? write.bind(8, *a.acquired_on) : write.bind_null(8));
            if (a.kind == K::UpdateSpecimen) {
                bound = bound && write.bind(9, a.replace_optional_metadata);
            }
        }

        if (!bound) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (write.step() != SQLITE_DONE) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (sqlite3_changes(_handle) != 1) {
            if (a.kind == K::DeleteCollection) {
                Query occupied;
                if (!occupied.prepare(_handle, "SELECT 1 FROM collection c JOIN specimen s ON s.collection_id=c.id WHERE c.id=? AND c.gardener_id=? LIMIT 1")) {
                    return fail(sqlite3_errmsg(_handle));
                }

                if (!occupied.bind(1, a.id)) {
                    return fail(sqlite3_errmsg(_handle));
                }

                if (!occupied.bind(2, gardener)) {
                    return fail(sqlite3_errmsg(_handle));
                }

                if (occupied.step() == SQLITE_ROW) {
                    return fail("Collection contains active or archived plants", mv::MainverteError::Code::CollectionNotEmpty);
                }
            }

            return fail("Object unavailable or invalid ownership", mv::MainverteError::Code::NotFound);
        }
    }

    mv::Plants result;
    Query collections;
    if (!collections.prepare(_handle, "SELECT id,name,created_at,modified_at FROM collection WHERE gardener_id=? ORDER BY id")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!collections.bind(1, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    int rc;
    while ((rc = collections.step()) == SQLITE_ROW) {
        result.collections.push_back({
            .id          = collections.column<int64_t>(0).value_or(0),
            .name        = std::string(collections.column<std::string_view>(1).value_or("")),
            .created_at  = collections.column<int64_t>(2).value_or(0),
            .modified_at = collections.column<int64_t>(3).value_or(0)});
    }

    if (rc != SQLITE_DONE) {
        return fail(sqlite3_errmsg(_handle));
    }

    Query reselection;
    if (!reselection.prepare(_handle, "UPDATE settings SET current_collection_id=(SELECT id FROM collection WHERE gardener_id=? ORDER BY id LIMIT 1) WHERE id=1 AND current_gardener_id=? AND current_collection_id IS NULL")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!reselection.bind(1, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!reselection.bind(2, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (reselection.step() != SQLITE_DONE) {
        return fail(sqlite3_errmsg(_handle));
    }

    Query selection;
    if (!selection.prepare(_handle, "SELECT current_collection_id FROM settings WHERE id=1 AND current_gardener_id=?")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!selection.bind(1, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (selection.step() != SQLITE_ROW) {
        return fail(sqlite3_errmsg(_handle));
    }

    result.selected = selection.column<int64_t>(0).value_or(0);
    if (a.kind == K::ListSpecimens) {
        Query accessible;
        if (!accessible.prepare(_handle, "SELECT id FROM collection WHERE id=? AND gardener_id=?")) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!accessible.bind(1, a.id)) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (!accessible.bind(2, gardener)) {
            return fail(sqlite3_errmsg(_handle));
        }

        if (accessible.step() != SQLITE_ROW) {
            return fail("Collection unavailable", mv::MainverteError::Code::NotFound);
        }
    }

    Query plants;
    if (!plants.prepare(_handle, "SELECT id,collection_id,display_name,botanical_name,cultivar,archived_at IS NOT NULL,created_at,modified_at,archived_at,profile,acquired_on FROM specimen WHERE collection_id IN(SELECT id FROM collection WHERE gardener_id=?3) AND ((?2=0 AND collection_id=?1) OR (?2>0 AND id=?2)) ORDER BY id DESC")) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!plants.bind(1, a.kind == K::ListSpecimens ? a.id : result.selected)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!plants.bind(2, a.kind == K::GetSpecimen ? a.id : 0)) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (!plants.bind(3, gardener)) {
        return fail(sqlite3_errmsg(_handle));
    }

    while ((rc = plants.step()) == SQLITE_ROW) {
        result.specimens.emplace_back(
            plants.column<int64_t>(0).value_or(0),
            plants.column<int64_t>(1).value_or(0),
            std::string(plants.column<std::string_view>(2).value_or("")),
            std::string(plants.column<std::string_view>(3).value_or("")),
            std::string(plants.column<std::string_view>(4).value_or("")),
            plants.column<bool>(5).value_or(false),
            plants.column<int64_t>(6).value_or(0),
            plants.column<int64_t>(7).value_or(0),
            plants.column<int64_t>(8),
            plants.column<mv::PlantProfile>(9),
            plants.column<std::string_view>(10).transform([](std::string_view value) { return std::string(value); })
        );
    }

    if (rc != SQLITE_DONE) {
        return fail(sqlite3_errmsg(_handle));
    }

    if (a.kind == K::GetSpecimen && (a.id <= 0 || result.specimens.empty())) {
        return fail("Specimen unavailable", mv::MainverteError::Code::NotFound);
    }

    if (!transaction_end(_handle)) {
        return fail(sqlite3_errmsg(_handle));
    }

    return result;
}
