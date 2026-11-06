// clazy:skip
#include <cstddef>
#include <initializer_list>

#include <doctest.h>
#include <sqlite3.h>

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

}  //  namespace

TEST_CASE("Migration commits schema and version together") {
    TestDatabase db;
    REQUIRE(detail::migrate_script(db.handle, "CREATE TABLE plants(id INTEGER);", 1));
    REQUIRE(detail::migrate_script(db.handle, "INSERT INTO plants VALUES(42);", 2));
    CHECK(db.scalar("SELECT id FROM plants;") == 42);
    CHECK(db.scalar("PRAGMA user_version;") == 2);
    CHECK(sqlite3_get_autocommit(db.handle) == 1);
}

TEST_CASE("Consolidated initial schema includes settings and specimen defaults") {
    TestDatabase db;
    REQUIRE(mv::sql::scripts.size() >= 1);
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    CHECK(db.scalar("PRAGMA user_version") == 1);
    CHECK(db.scalar("SELECT language='' AND dark_theme=0 AND log_level=0 AND debug_enabled=0 FROM settings") == 1);
    CHECK(db.scalar("SELECT count(*) FROM sqlite_master WHERE type='index' AND name IN ('collection_gardener','specimen_collection')") == 2);
    CHECK(db.scalar("SELECT count(*) FROM pragma_table_info('specimen') WHERE name IN ('species_id','location_id','photo_uri')") == 0);
    REQUIRE(sqlite3_exec(db.handle,
        "PRAGMA foreign_keys=ON;"
        "INSERT INTO gardener VALUES(7,'Camille',123);"
        "INSERT INTO collection VALUES(9,7,'Garden',123,123);"
        "UPDATE settings SET current_gardener_id=7,current_collection_id=9,language='fr',dark_theme=1,log_level=2,debug_enabled=1;"
        "INSERT INTO specimen(collection_id,display_name,created_at,modified_at) VALUES(9,'Nishiro',123,123);"
        "INSERT INTO specimen(collection_id,display_name,created_at,modified_at) VALUES(9,'Nishiro',123,123);",
        nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK(db.scalar("SELECT count(*) FROM specimen WHERE display_name='Nishiro'") == 2);
    CHECK(db.scalar("SELECT count(*) FROM specimen WHERE botanical_name='' AND cultivar='' AND archived_at IS NULL") == 2);
    CHECK(db.scalar("SELECT language='fr' AND dark_theme=1 AND log_level=2 AND debug_enabled=1 AND current_collection_id=9 FROM settings") == 1);
    CHECK(sqlite3_exec(db.handle, "DELETE FROM collection WHERE id=9", nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db.handle,
        "INSERT INTO specimen(collection_id,display_name,created_at,modified_at) VALUES(999,'Invalid',1,1)",
        nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db.handle,
        "INSERT INTO specimen(collection_id,display_name,created_at,modified_at) VALUES(9,'   ',1,1)",
        nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
}

TEST_CASE("Consolidated settings constraints reject invalid values without changing data") {
    TestDatabase db;
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    REQUIRE(sqlite3_exec(db.handle,
        "UPDATE settings SET language='fr',dark_theme=1,log_level=2,debug_enabled=1",
        nullptr, nullptr, nullptr) == SQLITE_OK);
    for (const auto* sql : {"UPDATE settings SET dark_theme=2", "UPDATE settings SET dark_theme=NULL",
                           "UPDATE settings SET log_level=-1", "UPDATE settings SET log_level=4",
                           "UPDATE settings SET log_level=NULL", "UPDATE settings SET log_level=1.5",
                           "UPDATE settings SET debug_enabled=-1", "UPDATE settings SET debug_enabled=2",
                           "UPDATE settings SET debug_enabled=NULL", "UPDATE settings SET debug_enabled=0.5",
                           "UPDATE settings SET language=NULL"}) {
        CHECK(sqlite3_exec(db.handle, sql, nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
    }

    CHECK(db.scalar("SELECT language='fr' AND dark_theme=1 AND log_level=2 AND debug_enabled=1 FROM settings") == 1);
}

TEST_CASE("Failed initial schema rolls back all new objects and preserves existing data") {
    TestDatabase db;
    REQUIRE(sqlite3_exec(db.handle, "CREATE TABLE specimen(marker INTEGER); INSERT INTO specimen VALUES(42)",
                         nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK_FALSE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    CHECK(db.scalar("PRAGMA user_version") == 0);
    CHECK(db.scalar("SELECT marker FROM specimen") == 42);
    CHECK(db.scalar("SELECT count(*) FROM sqlite_master WHERE name IN ('gardener','collection','settings','species','collection_gardener')") == 0);
    CHECK(sqlite3_get_autocommit(db.handle) == 1);
    REQUIRE(sqlite3_exec(db.handle, "DROP TABLE specimen", nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    CHECK(db.scalar("PRAGMA user_version") == 1);
    CHECK(db.scalar("SELECT count(*) FROM specimen") == 0);
}

TEST_CASE("Care migration preserves M1 specimens and adds optional metadata") {
    TestDatabase db;
    REQUIRE(mv::sql::scripts.size() == 2);
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    REQUIRE(sqlite3_exec(db.handle,
        "PRAGMA foreign_keys=ON;"
        "INSERT INTO gardener VALUES(7,'Camille',123);"
        "INSERT INTO collection VALUES(9,7,'Garden',123,124);"
        "UPDATE settings SET current_gardener_id=7,current_collection_id=9,language='fr';"
        "INSERT INTO specimen VALUES(42,9,'Nishiro','Gymnocalycium','Mihanovichii',100,200,300);",
        nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[1], 2));
    CHECK(db.scalar("PRAGMA user_version") == 2);
    CHECK(db.scalar("SELECT id=42 AND collection_id=9 AND display_name='Nishiro' AND botanical_name='Gymnocalycium' "
                    "AND cultivar='Mihanovichii' AND created_at=100 AND modified_at=200 AND archived_at=300 "
                    "AND profile IS NULL AND acquired_on IS NULL FROM specimen") == 1);
    CHECK(db.scalar("SELECT current_collection_id=9 AND language='fr' FROM settings") == 1);
    CHECK(db.scalar("SELECT count(*) FROM care_need") == 0);
    REQUIRE(sqlite3_exec(db.handle, "INSERT INTO care_need(specimen_id,type) VALUES(42,0),(42,0),(42,1)", nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK(db.scalar("SELECT count(DISTINCT id) FROM care_need WHERE specimen_id=42") == 3);
    CHECK(sqlite3_exec(db.handle, "INSERT INTO care_need(specimen_id,type) VALUES(999,0)", nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db.handle, "INSERT INTO care_need(specimen_id,type) VALUES(42,8)", nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_exec(db.handle, "UPDATE specimen SET profile=99", nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);
    CHECK(db.scalar("SELECT count(*) FROM pragma_foreign_key_check") == 0);
}

TEST_CASE("Care migration failure rolls back added columns and preserves the prior version") {
    TestDatabase db;
    REQUIRE(detail::migrate_script(db.handle, mv::sql::scripts[0], 1));
    REQUIRE(sqlite3_exec(db.handle, "CREATE TABLE care_need(marker); INSERT INTO care_need VALUES(42)", nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK_FALSE(detail::migrate_script(db.handle, mv::sql::scripts[1], 2));
    CHECK(db.scalar("PRAGMA user_version") == 1);
    CHECK(db.scalar("SELECT marker FROM care_need") == 42);
    CHECK(db.scalar("SELECT count(*) FROM pragma_table_info('specimen') WHERE name IN ('profile','acquired_on')") == 0);
    CHECK(sqlite3_get_autocommit(db.handle) == 1);
    REQUIRE(sqlite3_exec(db.handle, "DROP TABLE care_need", nullptr, nullptr, nullptr) == SQLITE_OK);
    CHECK(detail::migrate_script(db.handle, mv::sql::scripts[1], 2));
}
