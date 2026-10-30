// clazy:skip
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <string>
#include <system_error>

#include <sqlite3.h>
#include "doctest.h"

#include "database.h"

namespace {
struct Connection {
    sqlite3* handle = nullptr;
    ~Connection() { CHECK(sqlite3_close(handle) == SQLITE_OK); }

    std::string scalar(const char* query) {
        sqlite3_stmt* raw = nullptr;
        REQUIRE(sqlite3_prepare_v2(handle, query, -1, &raw, nullptr) == SQLITE_OK);
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, &sqlite3_finalize);
        REQUIRE(sqlite3_step(raw) == SQLITE_ROW);
        return reinterpret_cast<const char*>(sqlite3_column_text(raw, 0));
    }
};
}

TEST_CASE("Configuration verifies settings on an in-memory database") {
    REQUIRE(sqlite3_initialize() == SQLITE_OK);
    Connection db;
    REQUIRE(sqlite3_open(":memory:", &db.handle) == SQLITE_OK);
    REQUIRE(detail::configure_database(db.handle));
    CHECK(db.scalar("PRAGMA foreign_keys;") == "1");
    CHECK(db.scalar("PRAGMA journal_mode;") == "memory");
    CHECK(db.scalar("PRAGMA synchronous;") == "1");
    CHECK(db.scalar("PRAGMA cache_size;") == "-8192");
    CHECK(db.scalar("PRAGMA temp_store;") == "2");
    CHECK(db.scalar("PRAGMA busy_timeout;") == "3000");
}

TEST_CASE("Configuration rejects a silently ignored foreign-key setting") {
    REQUIRE(sqlite3_initialize() == SQLITE_OK);
    Connection db;
    REQUIRE(sqlite3_open(":memory:", &db.handle) == SQLITE_OK);
    REQUIRE(sqlite3_set_authorizer(db.handle,
        [](void*, int action, const char* name, const char* value, const char*, const char*) {
            return action == SQLITE_PRAGMA && name && std::strcmp(name, "foreign_keys") == 0 && value
                ? SQLITE_IGNORE : SQLITE_OK;
        }, nullptr) == SQLITE_OK);
    CHECK_FALSE(detail::configure_database(db.handle));
    CHECK(db.scalar("PRAGMA foreign_keys;") == "0");
}

TEST_CASE("Configuration enables WAL on a file database") {
    struct TemporaryDirectory {
        std::filesystem::path path;
        ~TemporaryDirectory() {
            std::error_code error;
            for (const char* name : {"database.db", "database.db-wal", "database.db-shm"}) {
                std::filesystem::remove(path / name, error);
                CHECK_FALSE(error);
            }
            std::filesystem::remove(path, error);
            CHECK_FALSE(error);
        }
    };
    const auto path = std::filesystem::temp_directory_path() / std::format(
        "mainverte-pragma-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    REQUIRE(std::filesystem::create_directory(path));
    TemporaryDirectory directory{path};
    const auto utf8_path = (path / "database.db").u8string();
    const std::string filename(utf8_path.begin(), utf8_path.end());
    REQUIRE(sqlite3_initialize() == SQLITE_OK);
    Connection db;
    REQUIRE(sqlite3_open(filename.c_str(), &db.handle) == SQLITE_OK);
    REQUIRE(detail::configure_database(db.handle));
    CHECK(db.scalar("PRAGMA journal_mode;") == "wal");
    CHECK(db.scalar("PRAGMA foreign_keys;") == "1");
}

TEST_CASE("Configuration reports a refused PRAGMA without aborting") {
    REQUIRE(sqlite3_initialize() == SQLITE_OK);
    Connection db;
    REQUIRE(sqlite3_open(":memory:", &db.handle) == SQLITE_OK);
    REQUIRE(sqlite3_set_authorizer(db.handle,
        [](void*, int action, const char*, const char*, const char*, const char*) {
            return action == SQLITE_PRAGMA ? SQLITE_DENY : SQLITE_OK;
        }, nullptr) == SQLITE_OK);
    CHECK_FALSE(detail::configure_database(db.handle));
}
