// clazy:skip
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <future>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include <doctest.h>
#include <sqlite3.h>

#include "database.h"
#include "mv_core.h"


namespace {

struct DatabaseFile {
    std::filesystem::path directory;
    std::string filename;

    DatabaseFile() {
        directory = std::filesystem::temp_directory_path() / std::format(
            "mainverte-lifecycle-{}", std::chrono::steady_clock::now().time_since_epoch().count());
        REQUIRE(std::filesystem::create_directory(directory));
        const auto utf8 = (directory / "database.db").u8string();
        filename.assign(utf8.begin(), utf8.end());
    }

    ~DatabaseFile() {
        std::error_code error;
        for (const char* name : {"database.db", "database.db-wal", "database.db-shm"}) {
            std::filesystem::remove(directory / name, error);
            CHECK_FALSE(error);
        }
        std::filesystem::remove(directory, error);
        CHECK_FALSE(error);
    }
};

struct Connection {
    sqlite3* handle = nullptr;
    explicit Connection(const std::string& filename) {
        REQUIRE(sqlite3_initialize() == SQLITE_OK);
        REQUIRE(sqlite3_open(filename.c_str(), &handle) == SQLITE_OK);
    }
    ~Connection() { CHECK(sqlite3_close(handle) == SQLITE_OK); }
};

// Return errors as data from the worker; doctest checks remain on the test thread.
std::pair<int, std::string> scalar(sqlite3* handle, const char* query) {
    sqlite3_stmt* raw = nullptr;
    int status = sqlite3_prepare_v2(handle, query, -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, &sqlite3_finalize);
    if (status != SQLITE_OK) {
        return {status, {}};
    }
    status = sqlite3_step(raw);
    if (status != SQLITE_ROW) {
        return {status, {}};
    }
    const auto* text = sqlite3_column_text(raw, 0);
    std::string value = text ? reinterpret_cast<const char*>(text) : "";
    status = sqlite3_finalize(statement.release());
    return {status, std::move(value)};
}

template<typename Job>
auto run_job(Database& database, Job job) {
    using Result = std::invoke_result_t<Job&, sqlite3*>;
    std::promise<Result> promise;
    auto result = promise.get_future();
    REQUIRE(database.submit([promise = std::move(promise), job = std::move(job)](sqlite3* handle) mutable {
        promise.set_value(job(handle));
    }));
    REQUIRE(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    return result.get();
}

}  // namespace

TEST_CASE("Reopening a database preserves user data and schema version") {
    DatabaseFile file;
    std::string version;
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        database.start();
        const auto initial = run_job(database, [](sqlite3* handle) { return scalar(handle, "PRAGMA user_version;"); });
        REQUIRE(initial.first == SQLITE_OK);
        version = initial.second;
        REQUIRE(std::stoi(version) > 0);
        CHECK(run_job(database, [](sqlite3* handle) {
            return sqlite3_exec(handle,
                "INSERT INTO gardener(id, display_name, created_at) VALUES(42, 'Persistent gardener', 123);",
                nullptr, nullptr, nullptr);
        }) == SQLITE_OK);
    }
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        database.start();
        const auto restored = run_job(database, [](sqlite3* handle) {
            return scalar(handle, "SELECT display_name FROM gardener WHERE id=42;");
        });
        REQUIRE(restored.first == SQLITE_OK);
        CHECK(restored.second == "Persistent gardener");
        const auto current = run_job(database, [](sqlite3* handle) { return scalar(handle, "PRAGMA user_version;"); });
        REQUIRE(current.first == SQLITE_OK);
        CHECK(current.second == version);
    }
}

TEST_CASE("An opening failure permits a successful retry on the same database object") {
    DatabaseFile file;
    const auto utf8 = file.directory.u8string();
    const std::string invalid_path(utf8.begin(), utf8.end());
    Database database;
    // Opening a directory as a database reaches sqlite3_open_v2 and fails.
    CHECK_FALSE(database.initialize(invalid_path));
    CHECK_FALSE(database.submit([](sqlite3*) {}));
    REQUIRE(database.initialize(file.filename));
    database.start();
    const auto result = run_job(database, [](sqlite3* handle) { return scalar(handle, "SELECT 42;"); });
    REQUIRE(result.first == SQLITE_OK);
    CHECK(result.second == "42");
}

TEST_CASE("Settings persist every field across database reopening and map zero to NULL") {
    DatabaseFile file;
    const mv::Settings settings{
        .dark_theme_enabled = true,
        .debug_enabled = true,
        .log_level = mv::LogLevel::Wrn,
        .current_gardener_id = 42,
        .current_collection_id = 73,
        .language = "fr",
    };
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        auto defaults = database.read_settings();
        REQUIRE(defaults.has_value());
        CHECK(defaults->current_gardener_id == 0);
        CHECK(defaults->current_collection_id == 0);
        CHECK_FALSE(defaults->dark_theme_enabled);
        CHECK_FALSE(defaults->debug_enabled);
        CHECK(defaults->log_level == mv::LogLevel::Dbg);
        database.start();
        REQUIRE(run_job(database, [](sqlite3* handle) {
            return sqlite3_exec(handle,
                "INSERT INTO gardener VALUES(42, 'Gardener', 123);"
                "INSERT INTO collection VALUES(73, 42, 'Collection', 123, 123);",
                nullptr, nullptr, nullptr);
        }) == SQLITE_OK);
        REQUIRE(run_job(database, [&database, settings](sqlite3*) {
            return database.write_settings(settings);
        }));
        REQUIRE(run_job(database, [&database, settings](sqlite3*) {
            return database.write_settings(settings);
        }));
    }
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        const auto restored = database.read_settings();
        REQUIRE(restored.has_value());
        CHECK(restored->current_gardener_id == 42);
        CHECK(restored->current_collection_id == 73);
        CHECK(restored->language == "fr");
        CHECK(restored->dark_theme_enabled);
        CHECK(restored->debug_enabled);
        CHECK(restored->log_level == mv::LogLevel::Wrn);
        auto updated = *restored;
        updated.dark_theme_enabled = false;
        updated.debug_enabled = false;
        updated.current_gardener_id = 0;
        REQUIRE(database.write_settings(updated));
        const auto partial = database.read_settings();
        REQUIRE(partial.has_value());
        CHECK(partial->current_gardener_id == 0);
        CHECK(partial->current_collection_id == 73);
        CHECK_FALSE(partial->dark_theme_enabled);
        CHECK_FALSE(partial->debug_enabled);
        CHECK(partial->log_level == mv::LogLevel::Wrn);
        REQUIRE(database.write_settings({}));
        database.start();
        const auto cleared = run_job(database, [](sqlite3* handle) {
            return scalar(handle, "SELECT current_gardener_id IS NULL AND current_collection_id IS NULL FROM settings;");
        });
        REQUIRE(cleared.first == SQLITE_OK);
        CHECK(cleared.second == "1");
    }
}

TEST_CASE("Settings errors preserve values and cached statements recover") {
    Database database;
    mv::Settings settings{};
    settings.dark_theme_enabled = true;
    settings.current_gardener_id = 42;
    settings.current_collection_id = 73;
    REQUIRE(database.initialize(":memory:"));
    database.start();
    REQUIRE(run_job(database, [](sqlite3* handle) {
        return sqlite3_exec(handle,
            "INSERT INTO gardener VALUES(42, 'Gardener', 123);"
            "INSERT INTO collection VALUES(73, 42, 'Collection', 123, 123);",
            nullptr, nullptr, nullptr);
    }) == SQLITE_OK);
    REQUIRE(run_job(database, [&database, settings](sqlite3*) {
        return database.write_settings(settings);
    }));
    auto invalid = settings;
    invalid.log_level = mv::LogLevel::Err;
    invalid.current_gardener_id = 0;
    invalid.current_collection_id = 999;
    CHECK_FALSE(run_job(database, [&database, invalid](sqlite3*) {
        return database.write_settings(invalid);
    }));
    auto retained = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    REQUIRE(retained.has_value());
    CHECK(retained->current_gardener_id == 42);
    CHECK(retained->current_collection_id == 73);
    CHECK(retained->dark_theme_enabled);
    CHECK(run_job(database, [](sqlite3* handle) {
        for (auto* statement = sqlite3_next_stmt(handle, nullptr); statement;
             statement = sqlite3_next_stmt(handle, statement)) {
            if (sqlite3_stmt_busy(statement)) {
                return false;
            }
        }
        return true;
    }));
    REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({}); }));
    REQUIRE(run_job(database, [](sqlite3* handle) {
        return sqlite3_exec(handle, "DELETE FROM settings;", nullptr, nullptr, nullptr);
    }) == SQLITE_OK);
    CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.read_settings(); }).has_value());
    CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.write_settings({}); }));
    REQUIRE(run_job(database, [](sqlite3* handle) {
        return sqlite3_exec(handle, "INSERT INTO settings(id) VALUES(1);", nullptr, nullptr, nullptr);
    }) == SQLITE_OK);
    REQUIRE(run_job(database, [&database, settings](sqlite3*) {
        return database.write_settings(settings);
    }));
    retained = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    REQUIRE(retained.has_value());
    CHECK(retained->current_gardener_id == 42);
    CHECK(retained->current_collection_id == 73);
    CHECK(retained->dark_theme_enabled);
    REQUIRE(run_job(database, [](sqlite3* handle) {
        return sqlite3_exec(handle, "DROP TABLE settings;", nullptr, nullptr, nullptr);
    }) == SQLITE_OK);
    CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.read_settings(); }).has_value());
    CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.write_settings({}); }));
}

TEST_CASE("Unsupported schema versions are rejected without modifying existing data") {
    const char* version_sql = nullptr;
    const char* expected_version = nullptr;
    SUBCASE("A schema newer than this software") {
        version_sql = "PRAGMA user_version=2147483647;";
        expected_version = "2147483647";
    }
#ifdef NDEBUG
    // Debug intentionally asserts on a negative version; test the error result in Release.
    SUBCASE("A negative schema version") {
        version_sql = "PRAGMA user_version=-1;";
        expected_version = "-1";
    }
#endif
    REQUIRE(version_sql != nullptr);
    DatabaseFile file;
    std::string schema;
    {
        Connection seed(file.filename);
        REQUIRE(sqlite3_exec(seed.handle,
            "CREATE TABLE sentinel(value TEXT); INSERT INTO sentinel VALUES('keep me');",
            nullptr, nullptr, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_exec(seed.handle, version_sql, nullptr, nullptr, nullptr) == SQLITE_OK);
        const auto original = scalar(seed.handle, "SELECT sql FROM sqlite_schema WHERE name='sentinel';");
        REQUIRE(original.first == SQLITE_OK);
        schema = original.second;
    }
    {
        Database database;
        CHECK_FALSE(database.initialize(file.filename));
        CHECK_FALSE(database.submit([](sqlite3*) {}));
    }
    Connection persisted(file.filename);
    const auto version = scalar(persisted.handle, "PRAGMA user_version;");
    REQUIRE(version.first == SQLITE_OK);
    CHECK(version.second == expected_version);
    const auto row = scalar(persisted.handle, "SELECT value FROM sentinel;");
    REQUIRE(row.first == SQLITE_OK);
    CHECK(row.second == "keep me");
    const auto preserved_schema = scalar(persisted.handle, "SELECT sql FROM sqlite_schema WHERE name='sentinel';");
    REQUIRE(preserved_schema.first == SQLITE_OK);
    CHECK(preserved_schema.second == schema);
    // PRAGMA optimize may create SQLite's own statistics tables.
    const auto table_count = scalar(persisted.handle,
        "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name NOT GLOB 'sqlite_*';");
    REQUIRE(table_count.first == SQLITE_OK);
    CHECK(table_count.second == "1");
}

TEST_CASE("Log levels round trip and invalid writes preserve persisted settings") {
    Database database;
    REQUIRE(database.initialize(":memory:"));
    mv::Settings settings;
    settings.language = "fr";
    for (const auto level : {mv::LogLevel::Dbg, mv::LogLevel::Inf, mv::LogLevel::Wrn, mv::LogLevel::Err}) {
        settings.log_level = level;
        REQUIRE(database.write_settings(settings));
        const auto restored = database.read_settings();
        REQUIRE(restored.has_value());
        CHECK(restored->log_level == level);
    }

    for (const auto value : {-1, 4, 127}) {
        auto invalid = settings;
        invalid.log_level = static_cast<mv::LogLevel>(value);
        invalid.language = "en";
        CHECK_FALSE(database.write_settings(invalid));
        const auto retained = database.read_settings();
        REQUIRE(retained.has_value());
        CHECK(retained->log_level == mv::LogLevel::Err);
        CHECK(retained->language == "fr");
    }

    REQUIRE(database.write_settings({}));
    REQUIRE(database.read_settings().has_value());
    CHECK(database.read_settings()->log_level == mv::LogLevel::Dbg);
}

TEST_CASE("Invalid stored log levels are rejected without narrowing") {
    Database database;
    REQUIRE(database.initialize(":memory:"));
    database.start();
    for (const auto* value : {"-1", "4", "256", "1.5", "'invalid'"}) {
        const auto sql = std::format("PRAGMA ignore_check_constraints=ON; UPDATE settings SET log_level={};", value);
        REQUIRE(run_job(database, [sql](sqlite3* handle) {
            return sqlite3_exec(handle, sql.c_str(), nullptr, nullptr, nullptr);
        }) == SQLITE_OK);
        CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.read_settings(); }).has_value());
    }

    REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({}); }));
    const auto recovered = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    REQUIRE(recovered.has_value());
    CHECK(recovered->log_level == mv::LogLevel::Dbg);
}

TEST_CASE("Invalid stored debug flags are rejected and the cached statement recovers") {
    Database database;
    REQUIRE(database.initialize(":memory:"));
    database.start();
    for (const auto* value : {"-1", "2", "256", "0.5", "'invalid'"}) {
        const auto sql = std::format("PRAGMA ignore_check_constraints=ON; UPDATE settings SET debug_enabled={};", value);
        REQUIRE(run_job(database, [sql](sqlite3* handle) {
            return sqlite3_exec(handle, sql.c_str(), nullptr, nullptr, nullptr);
        }) == SQLITE_OK);
        CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.read_settings(); }).has_value());
    }

    REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({}); }));
    const auto recovered = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    REQUIRE(recovered.has_value());
    CHECK_FALSE(recovered->debug_enabled);
}

TEST_CASE("Query decodes columns strictly without SQLite coercion") {
    Connection db(":memory:");
    Query query;
    REQUIRE(query.prepare(db.handle, "SELECT 7, 1.5, 'text', NULL, 256, 1, 0, 2, -1"));
    REQUIRE(query.step() == SQLITE_ROW);

    const auto integer = query.column<int64_t>(0);
    REQUIRE(integer.has_value());
    CHECK(*integer == 7);
    CHECK_FALSE(query.column<int64_t>(1).has_value());  // REAL is not truncated to 1.
    CHECK_FALSE(query.column<int64_t>(2).has_value());  // TEXT is not read as 0.
    CHECK_FALSE(query.column<int64_t>(3).has_value());  // NULL stays distinct from 0.

    const auto yes = query.column<bool>(5);
    const auto no = query.column<bool>(6);
    REQUIRE(yes.has_value());
    REQUIRE(no.has_value());
    CHECK(*yes);
    CHECK_FALSE(*no);
    for (const int column : {1, 2, 3, 4, 7, 8}) {
        CAPTURE(column);
        CHECK_FALSE(query.column<bool>(column).has_value());  // Only 0 and 1 are booleans.
    }
}

TEST_CASE("Stored selection that does not resolve to an existing row reads as no selection") {
    Database database;
    REQUIRE(database.initialize(":memory:"));
    database.start();
    const auto execute = [&database](const char* sql) {
        return run_job(database, [sql](sqlite3* handle) {
            return sqlite3_exec(handle, sql, nullptr, nullptr, nullptr);
        });
    };
    REQUIRE(execute("INSERT INTO gardener VALUES(1,'Ada',1);"
                    "INSERT INTO collection VALUES(10,1,'Ada plants',1,1);") == SQLITE_OK);
    const auto stored = [&](const char* update) {
        REQUIRE(execute(update) == SQLITE_OK);
        return run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    };

    auto valid = stored("UPDATE settings SET current_gardener_id=1, current_collection_id=10;");
    REQUIRE(valid.has_value());
    CHECK(valid->current_gardener_id == 1);
    CHECK(valid->current_collection_id == 10);

    // Dangling references can only be stored with foreign keys disabled.
    auto missing_collection = stored("PRAGMA foreign_keys=OFF;"
                                     "UPDATE settings SET current_gardener_id=1, current_collection_id=999;"
                                     "PRAGMA foreign_keys=ON;");
    REQUIRE(missing_collection.has_value());
    CHECK(missing_collection->current_gardener_id == 1);
    CHECK(missing_collection->current_collection_id == 0);

    auto missing_gardener = stored("PRAGMA foreign_keys=OFF;"
                                   "UPDATE settings SET current_gardener_id=99, current_collection_id=NULL;"
                                   "PRAGMA foreign_keys=ON;");
    REQUIRE(missing_gardener.has_value());
    CHECK(missing_gardener->current_gardener_id == 0);
    CHECK(missing_gardener->current_collection_id == 0);

    auto missing_both = stored("PRAGMA foreign_keys=OFF;"
                               "UPDATE settings SET current_gardener_id=99, current_collection_id=999;"
                               "PRAGMA foreign_keys=ON;");
    REQUIRE(missing_both.has_value());
    CHECK(missing_both->current_gardener_id == 0);
    CHECK(missing_both->current_collection_id == 0);
}

TEST_CASE("Settings and gardener operations stay usable after refused or failed attempts") {
    Database database;
    REQUIRE(database.initialize(":memory:"));
    database.start();
    const auto execute = [&database](const char* sql) {
        return run_job(database, [sql](sqlite3* handle) {
            return sqlite3_exec(handle, sql, nullptr, nullptr, nullptr);
        });
    };
    const auto write = [&database](const mv::Settings& settings) {
        return run_job(database, [&database, settings](sqlite3*) { return database.write_settings(settings); });
    };
    const auto read = [&database] {
        return run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    };
    REQUIRE(execute("INSERT INTO gardener VALUES(1,'Ada',1),(2,'Bob',1);"
                    "INSERT INTO collection VALUES(20,2,'Bob plants',1,1);") == SQLITE_OK);

    mv::Settings settings;
    settings.language = "fr";
    settings.current_gardener_id = 1;

    // SQLite refuses the update itself.
    REQUIRE(execute("CREATE TRIGGER refuse_settings BEFORE UPDATE ON settings "
                    "BEGIN SELECT RAISE(ABORT,'refused'); END;") == SQLITE_OK);
    CHECK_FALSE(write(settings));
    REQUIRE(execute("DROP TRIGGER refuse_settings;") == SQLITE_OK);

    // Core refuses a collection owned by another gardener before updating anything.
    auto foreign = settings;
    foreign.language = "de";
    foreign.current_collection_id = 20;
    CHECK_FALSE(write(foreign));
    auto untouched = read();
    REQUIRE(untouched.has_value());
    CHECK(untouched->language.empty());
    CHECK(untouched->current_gardener_id == 0);

    // The next valid write applies completely; nothing from the refused attempts leaks into it.
    REQUIRE(write(settings));
    auto stored = read();
    REQUIRE(stored.has_value());
    CHECK(stored->language == "fr");
    CHECK(stored->current_gardener_id == 1);
    CHECK(stored->current_collection_id == 0);

    REQUIRE(execute("CREATE TRIGGER refuse_gardener BEFORE INSERT ON gardener "
                    "BEGIN SELECT RAISE(ABORT,'refused'); END;") == SQLITE_OK);
    CHECK(run_job(database, [&database](sqlite3*) { return database.create_gardener("Cleo"); }) == -1);
    REQUIRE(execute("DROP TRIGGER refuse_gardener;") == SQLITE_OK);
    const auto created = run_job(database, [&database](sqlite3*) { return database.create_gardener("Dan"); });
    REQUIRE(created > 0);
    const auto name = run_job(database, [created](sqlite3* handle) {
        return scalar(handle, std::format("SELECT display_name FROM gardener WHERE id={};", created).c_str());
    });
    REQUIRE(name.first == SQLITE_OK);
    CHECK(name.second == "Dan");
}

namespace {

int exec(sqlite3* handle, const char* sql) {
    return sqlite3_exec(handle, sql, nullptr, nullptr, nullptr);
}

// Values of column "b" (NULL reads as "null") for rows ordered by a, joined with commas.
std::string rows(sqlite3* handle) {
    Query select;
    std::string out;
    if (!select.prepare(handle, "SELECT a, b FROM t ORDER BY a")) {
        return "prepare failed";
    }

    while (select.step() == SQLITE_ROW) {
        const auto a = select.column<int64_t>(0);
        const auto b = select.column<std::string_view>(1);
        out += std::format("{}{}={}", out.empty() ? "" : ",", a.value_or(-1), b ? std::string(*b) : "null");
    }

    return out;
}

}  // namespace

TEST_CASE("Query reuse: repeated prepare and execute rebind different values") {
    Connection connection(":memory:");
    REQUIRE(exec(connection.handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);") == SQLITE_OK);

    Query insert;
    static constexpr const char* sql = "INSERT INTO t(a,b) VALUES(?1,?2)";
    for (int64_t i = 1; i <= 3; ++i) {
        REQUIRE(insert.prepare(connection.handle, sql));
        REQUIRE(insert.bind(1, i));
        REQUIRE(insert.bind(2, std::string_view(i == 2 ? "two" : "other")));
        CHECK(insert.step() == SQLITE_DONE);
    }

    CHECK(rows(connection.handle) == "1=other,2=two,3=other");
    CHECK(sqlite3_changes(connection.handle) == 1);
}

TEST_CASE("Query reuse: no binding survives into the next execution") {
    Connection connection(":memory:");
    REQUIRE(exec(connection.handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);") == SQLITE_OK);

    Query insert;
    static constexpr const char* sql = "INSERT INTO t(a,b) VALUES(?1,?2)";
    REQUIRE(insert.prepare(connection.handle, sql));
    REQUIRE(insert.bind(1, int64_t{1}));
    REQUIRE(insert.bind(2, std::string_view("kept?")));
    REQUIRE(insert.step() == SQLITE_DONE);

    // sqlite3_reset() alone would leave ?2 = 'kept?' bound; the reuse must start from NULLs.
    REQUIRE(insert.prepare(connection.handle, sql));
    REQUIRE(insert.bind(1, int64_t{2}));
    REQUIRE(insert.step() == SQLITE_DONE);

    CHECK(rows(connection.handle) == "1=kept?,2=null");
}

TEST_CASE("Query reuse: a failed execution does not leak into the next one") {
    Connection connection(":memory:");
    REQUIRE(exec(connection.handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);"
                                    "INSERT INTO t VALUES(1,'first');") == SQLITE_OK);

    Query insert;
    static constexpr const char* sql = "INSERT INTO t(a,b) VALUES(?1,?2)";
    REQUIRE(insert.prepare(connection.handle, sql));
    REQUIRE(insert.bind(1, int64_t{1}));
    REQUIRE(insert.bind(2, std::string_view("duplicate")));
    CHECK((insert.step() & 0xff) == SQLITE_CONSTRAINT);

    // The failure was reported by step(). Preparing again must succeed (the reset result that
    // repeats it is not an error of the new use) and the new execution reports its own outcome.
    REQUIRE(insert.prepare(connection.handle, sql));
    REQUIRE(insert.bind(1, int64_t{2}));
    REQUIRE(insert.bind(2, std::string_view("second")));
    CHECK(insert.step() == SQLITE_DONE);
    CHECK(rows(connection.handle) == "1=first,2=second");

    // A second failure after a success is reported again, with its own message.
    REQUIRE(insert.prepare(connection.handle, sql));
    REQUIRE(insert.bind(1, int64_t{2}));
    CHECK((insert.step() & 0xff) == SQLITE_CONSTRAINT);
    CHECK(std::string(sqlite3_errmsg(connection.handle)).find("UNIQUE") != std::string::npos);
}

TEST_CASE("Query reuse: a statement left on a row restarts from the first row") {
    Connection connection(":memory:");
    REQUIRE(exec(connection.handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);"
                                    "INSERT INTO t VALUES(1,'x'),(2,'y');") == SQLITE_OK);

    Query select;
    static constexpr const char* sql = "SELECT a FROM t WHERE a >= ?1 ORDER BY a";
    REQUIRE(select.prepare(connection.handle, sql));
    REQUIRE(select.bind(1, int64_t{1}));
    REQUIRE(select.step() == SQLITE_ROW);
    CHECK(select.column<int64_t>(0) == 1);

    REQUIRE(select.prepare(connection.handle, sql));
    REQUIRE(select.bind(1, int64_t{2}));
    REQUIRE(select.step() == SQLITE_ROW);
    CHECK(select.column<int64_t>(0) == 2);
    CHECK(select.step() == SQLITE_DONE);

    select.reset();
    select.reset();  // Idempotent.
    REQUIRE(select.prepare(connection.handle, sql));
    CHECK(select.step() == SQLITE_DONE);  // ?1 is NULL again: nothing matches.
}

TEST_CASE("Query reuse: different SQL is refused and never executed") {
    Connection connection(":memory:");
    REQUIRE(exec(connection.handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);") == SQLITE_OK);

    Query query;
    REQUIRE(query.prepare(connection.handle, "INSERT INTO t(a) VALUES(?1)"));
    CHECK_FALSE(query.prepare(connection.handle, "DELETE FROM t"));

    // The refusal leaves the original statement usable and untouched.
    REQUIRE(query.bind(1, int64_t{7}));
    CHECK(query.step() == SQLITE_DONE);
    CHECK(rows(connection.handle) == "7=null");

    // Same text is accepted again; a prefix or extension of it is a different statement.
    CHECK(query.prepare(connection.handle, "INSERT INTO t(a) VALUES(?1)"));
    CHECK_FALSE(query.prepare(connection.handle, "INSERT INTO t(a) VALUES(?1) "));
    CHECK_FALSE(query.prepare(connection.handle, "INSERT INTO t(a) VALUES(?1"));
}

TEST_CASE("Query reuse: another connection is refused and never executed") {
    Connection first(":memory:");
    Connection second(":memory:");
    for (sqlite3* handle : {first.handle, second.handle}) {
        REQUIRE(exec(handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);") == SQLITE_OK);
    }

    Query query;
    static constexpr const char* sql = "INSERT INTO t(a) VALUES(?1)";
    REQUIRE(query.prepare(first.handle, sql));
    CHECK_FALSE(query.prepare(second.handle, sql));

    REQUIRE(query.bind(1, int64_t{1}));
    CHECK(query.step() == SQLITE_DONE);
    CHECK(rows(first.handle) == "1=null");
    CHECK(rows(second.handle).empty());
}

TEST_CASE("Query cleanup: finalize releases the statement before its connection closes") {
    {
        // Connection's destructor requires sqlite3_close() == SQLITE_OK, which fails while any
        // statement is alive: this pins that finalize() really finalizes.
        Connection connection(":memory:");
        REQUIRE(exec(connection.handle, "CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);") == SQLITE_OK);

        Query query;
        static constexpr const char* sql = "SELECT a FROM t";
        REQUIRE(query.prepare(connection.handle, sql));
        CHECK(sqlite3_next_stmt(connection.handle, nullptr) != nullptr);
        query.finalize();
        query.finalize();  // Idempotent.
        CHECK(sqlite3_next_stmt(connection.handle, nullptr) == nullptr);

        // A finalized Query is empty again: it can serve another SQL text and another connection.
        Connection other(":memory:");
        REQUIRE(exec(other.handle, "CREATE TABLE u(x);") == SQLITE_OK);
        REQUIRE(query.prepare(other.handle, "SELECT x FROM u"));
        CHECK(query.step() == SQLITE_DONE);
        query.finalize();
    }

    // A Query that is destroyed (not finalized explicitly) releases its statement too.
    Connection connection(":memory:");
    {
        Query query;
        REQUIRE(query.prepare(connection.handle, "SELECT 1"));
    }
    CHECK(sqlite3_next_stmt(connection.handle, nullptr) == nullptr);
}

TEST_CASE("Database keeps its settings statements across operations and shuts down cleanly") {
    DatabaseFile file;
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        database.start();

        auto settings = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
        REQUIRE(settings.has_value());
        for (int i = 0; i < 3; ++i) {
            settings->language = i % 2 == 0 ? "fr" : "en";
            REQUIRE(run_job(database, [&database, &settings](sqlite3*) { return database.write_settings(*settings); }));
            const auto stored = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
            REQUIRE(stored.has_value());
            CHECK(stored->language == settings->language);
        }

        // The persistent statements stay prepared between jobs but hold no row: a write from
        // another connection must not be blocked by a read lock left behind.
        CHECK(run_job(database, [](sqlite3* handle) { return sqlite3_next_stmt(handle, nullptr) != nullptr; }));
        Connection other(file.filename);
        CHECK(exec(other.handle, "BEGIN IMMEDIATE; COMMIT;") == SQLITE_OK);

        // A read that fails while the statement is on its row (invalid stored flag) must still
        // leave no read lock: a TRUNCATE checkpoint reports busy = 1 if one remains.
        REQUIRE(run_job(database, [](sqlite3* handle) {
            return exec(handle, "PRAGMA ignore_check_constraints=ON; UPDATE settings SET debug_enabled=2;");
        }) == SQLITE_OK);
        CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.read_settings(); }).has_value());
        const auto checkpoint = scalar(other.handle, "PRAGMA wal_checkpoint(TRUNCATE);");
        CHECK(checkpoint.first == SQLITE_OK);
        CHECK(checkpoint.second == "0");

        REQUIRE(run_job(database, [](sqlite3* handle) {
            return exec(handle, "UPDATE settings SET debug_enabled=0; PRAGMA ignore_check_constraints=OFF;");
        }) == SQLITE_OK);
        CHECK(run_job(database, [&database](sqlite3*) { return database.read_settings(); }).has_value());
    }

    // The database destructor finalized its statements and closed the connection (ASan/leak
    // detection and the debug assertion would flag otherwise); the file can be reopened.
    Connection reopened(file.filename);
    CHECK(scalar(reopened.handle, "SELECT language FROM settings WHERE id=1").first == SQLITE_OK);
}
