// clazy:skip
#include <chrono>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

#include <future>
#include <sqlite3.h>

#include "database.h"
#include "doctest.h"

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
}

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
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        auto defaults = database.read_settings();
        REQUIRE(defaults.has_value());
        CHECK(defaults->current_gardener_id == 0);
        CHECK(defaults->current_collection_id == 0);
        database.start();
        REQUIRE(run_job(database, [](sqlite3* handle) {
            return sqlite3_exec(handle,
                "INSERT INTO gardener VALUES(42, 'Gardener', 123);"
                "INSERT INTO collection VALUES(73, 42, 'Collection', 123, 123);",
                nullptr, nullptr, nullptr);
        }) == SQLITE_OK);
        REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({42, 73, "fr"}); }));
        REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({42, 73, "fr"}); }));
    }
    {
        Database database;
        REQUIRE(database.initialize(file.filename));
        const auto restored = database.read_settings();
        REQUIRE(restored.has_value());
        CHECK(restored->current_gardener_id == 42);
        CHECK(restored->current_collection_id == 73);
        REQUIRE(database.write_settings({0, 73, "fr"}));
        const auto partial = database.read_settings();
        REQUIRE(partial.has_value());
        CHECK(partial->current_gardener_id == 0);
        CHECK(partial->current_collection_id == 73);
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
    REQUIRE(database.initialize(":memory:"));
    database.start();
    REQUIRE(run_job(database, [](sqlite3* handle) {
        return sqlite3_exec(handle,
            "INSERT INTO gardener VALUES(42, 'Gardener', 123);"
            "INSERT INTO collection VALUES(73, 42, 'Collection', 123, 123);",
            nullptr, nullptr, nullptr);
    }) == SQLITE_OK);
    REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({42, 73, "fr"}); }));
    CHECK_FALSE(run_job(database, [&database](sqlite3*) { return database.write_settings({0, 999, "fr"}); }));
    auto retained = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    REQUIRE(retained.has_value());
    CHECK(retained->current_gardener_id == 42);
    CHECK(retained->current_collection_id == 73);
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
    REQUIRE(run_job(database, [&database](sqlite3*) { return database.write_settings({42, 73, "fr"}); }));
    retained = run_job(database, [&database](sqlite3*) { return database.read_settings(); });
    REQUIRE(retained.has_value());
    CHECK(retained->current_gardener_id == 42);
    CHECK(retained->current_collection_id == 73);
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
