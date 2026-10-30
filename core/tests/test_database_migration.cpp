// clazy:skip
#include <sqlite3.h>
#include "doctest.h"

#include "database.h"
#include "sql_scripts.hpp"

namespace {
struct TestDatabase {
    sqlite3* handle = nullptr;
    TestDatabase() {
        REQUIRE(sqlite3_initialize() == SQLITE_OK);
        REQUIRE(sqlite3_open(":memory:", &handle) == SQLITE_OK);
    }
    ~TestDatabase() { sqlite3_close(handle); }

    int scalar(const char* sql) {
        sqlite3_stmt* statement = nullptr;
        const int prepared = sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr);
        CHECK(prepared == SQLITE_OK);
        int value = -1;
        if (prepared == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
            value = sqlite3_column_int(statement, 0);
        }
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
        return value;
    }
};
}

TEST_CASE("Migration commits schema and version together") {
    TestDatabase db;
    REQUIRE(detail::migrate_script(db.handle, "CREATE TABLE plants(id INTEGER);", 1));
    REQUIRE(detail::migrate_script(db.handle, "INSERT INTO plants VALUES(42);", 2));
    CHECK(db.scalar("SELECT id FROM plants;") == 42);
    CHECK(db.scalar("PRAGMA user_version;") == 2);
    CHECK(sqlite3_get_autocommit(db.handle) == 1);
}

TEST_CASE("Language migration preserves existing gardener and collection settings") {
    TestDatabase db;
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    REQUIRE(sqlite3_exec(db.handle,
        "INSERT INTO gardener VALUES(7, 'Camille', 0);"
        "INSERT INTO collection VALUES(9, 7, 'Garden', 0, 0);"
        "UPDATE settings SET current_gardener_id=7, current_collection_id=9;",
        nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[1], 2));
    CHECK(db.scalar("PRAGMA user_version") == 2);
    CHECK(db.scalar("SELECT current_gardener_id FROM settings") == 7);
    CHECK(db.scalar("SELECT current_collection_id FROM settings") == 9);
    CHECK(db.scalar("SELECT count(*) FROM gardener WHERE display_name='Camille'") == 1);
    CHECK(db.scalar("SELECT language = '' FROM settings") == 1);
}
