// clazy:skip
#include <chrono>
#include <deque>
#include <filesystem>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <doctest.h>
#include <sqlite3.h>

#include "database.h"
#include "mv_core.h"
#include "mv_definitions.h"
#include "mv_move_only_function.h"
#include "services.h"


namespace {

std::mutex completion_mutex;
std::deque<mv::MoveOnlyFunction<void()>> completions;

void deliver_completions() {
    std::deque<mv::MoveOnlyFunction<void()>> pending;
    {
        std::lock_guard lock(completion_mutex);
        pending.swap(completions);
    }
    for (auto& callback : pending) {
        callback();
    }
}

int execute_on_worker(const char* sql) {
    std::promise<int> completed;
    auto result = completed.get_future();
    if (!service::database().submit([sql, completed = std::move(completed)](sqlite3* handle) mutable {
            completed.set_value(sqlite3_exec(handle, sql, nullptr, nullptr, nullptr));
        })) return SQLITE_MISUSE;
    // Joining Services during stack unwinding also completes any accepted job.
    if (result.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        return SQLITE_BUSY;
    }
    return result.get();
}

}  // namespace

void mv::platform::dispatch_callback(MoveOnlyFunction<void()> callback) {
    std::lock_guard lock(completion_mutex);
    completions.push_back(std::move(callback));
}

TEST_CASE("create_gardener persists names and dispatches success and database errors") {
    struct DatabaseFile {
        std::filesystem::path directory = std::filesystem::temp_directory_path() / std::format(
            "mainverte-services-{}", std::chrono::steady_clock::now().time_since_epoch().count());
        ~DatabaseFile() {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            CHECK_FALSE(error);
        }
    } file;
    const auto utf8 = (file.directory / "database.db").u8string();
    const std::string filename(utf8.begin(), utf8.end());
    {
        Database seed;
        REQUIRE(seed.initialize(filename));
        seed.start();
        std::promise<int> completed;
        auto result = completed.get_future();
        REQUIRE(seed.submit([completed = std::move(completed)](sqlite3* handle) mutable {
            completed.set_value(sqlite3_exec(handle,
                "INSERT INTO gardener VALUES(42, 'Seed', 123);"
                "INSERT INTO collection VALUES(73, 42, 'Collection', 123, 123);"
                "UPDATE settings SET current_gardener_id=42, current_collection_id=73;",
                nullptr, nullptr, nullptr));
        }));
        REQUIRE(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        REQUIRE(result.get() == SQLITE_OK);
    }
    mv::Services services;
    REQUIRE(services.initialize(filename));
    auto settings = mv::settings();
    CHECK(settings.current_gardener_id == 42);
    CHECK(settings.current_collection_id == 73);
    settings.current_gardener_id = 0;
    settings.log_level = mv::LogLevel::Wrn;
    CHECK(mv::settings().current_gardener_id == 42);
    REQUIRE(mv::save_settings(settings));
    CHECK(mv::settings().current_gardener_id == 42);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    CHECK(mv::settings().current_gardener_id == 0);
    CHECK(mv::settings().current_collection_id == 73);
    CHECK(mv::settings().log_level == mv::LogLevel::Wrn);
    REQUIRE(execute_on_worker(
        "CREATE TRIGGER check_settings BEFORE INSERT ON gardener BEGIN "
        "SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM settings WHERE current_gardener_id IS NULL "
        "AND current_collection_id=73 AND log_level=2) THEN RAISE(ABORT, 'settings not saved') END; END;") == SQLITE_OK);
    auto invalid_settings = mv::settings();
    invalid_settings.log_level = mv::LogLevel::Err;
    invalid_settings.current_gardener_id = 42;
    invalid_settings.current_collection_id = 999;
    REQUIRE(mv::save_settings(invalid_settings));
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    CHECK(mv::settings().current_gardener_id == 0);
    CHECK(mv::settings().current_collection_id == 73);
    CHECK(mv::settings().log_level == mv::LogLevel::Wrn);
    const auto caller = std::this_thread::get_id();
    std::optional<mv::GardenerResult> received;
    int calls = 0;
    auto callback = [&](mv::GardenerResult result) {
        CHECK(std::this_thread::get_id() == caller);
        ++calls;
        received = std::move(result);
    };

    // Quotes and SQL-looking text must be stored as values, not interpreted.
    const std::string name = "MV'); DROP TABLE gardener; --";
    mv::create_gardener(name, callback);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    CHECK(calls == 0);
    deliver_completions();
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());
    const auto first_id = received->value().id;
    CHECK(first_id > 0);
    CHECK(received->value().name == name);
    CHECK(calls == 1);

    // A trigger checks the first row and its timestamp on the SQLite worker.
    REQUIRE(execute_on_worker(
        "CREATE TRIGGER check_previous BEFORE INSERT ON gardener BEGIN "
        "SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM gardener WHERE "
        "display_name = 'MV''); DROP TABLE gardener; --' "
        "AND created_at > 0) THEN RAISE(ABORT, 'missing previous gardener') END; END;") == SQLITE_OK);
    mv::create_gardener("Second", callback);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    REQUIRE(received->has_value());
    CHECK(received->value().id != first_id);
    CHECK(received->value().name == "Second");
    CHECK(calls == 2);

    REQUIRE(execute_on_worker(
        "CREATE TRIGGER reject_gardener BEFORE INSERT ON gardener BEGIN "
        "SELECT RAISE(ABORT, 'test rejection'); END;") == SQLITE_OK);
    mv::create_gardener("Rejected", callback);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    REQUIRE_FALSE(received->has_value());
    CHECK(received->error().code == mv::MainverteError::Code::Database);
    CHECK(received->error().message.find("test rejection") != std::string::npos);
    CHECK(calls == 3);

    REQUIRE(mv::save_settings({}));
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    CHECK(mv::settings().current_gardener_id == 0);
    CHECK(mv::settings().current_collection_id == 0);
    REQUIRE(execute_on_worker("DROP TABLE gardener;") == SQLITE_OK);
    mv::create_gardener("Missing table", callback);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    REQUIRE_FALSE(received->has_value());
    CHECK(received->error().code == mv::MainverteError::Code::Database);
    CHECK(received->error().message.find("no such table") != std::string::npos);
    CHECK(calls == 4);
}

TEST_CASE("Setup refuses an empty name without scheduling a completion") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    bool called = false;
    CHECK_FALSE(mv::complete_setup("", "fr", [&](mv::GardenerResult) { called = true; }));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    deliver_completions();
    CHECK_FALSE(called);
    CHECK(mv::settings().current_gardener_id == 0);
    CHECK(mv::settings().language.empty());
}

TEST_CASE("Setup preserves the system language preference and creates no collection") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    std::optional<mv::GardenerResult> received;
    REQUIRE(mv::complete_setup("Alice", "", [&](mv::GardenerResult result) {
        received = std::move(result);
    }));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    CHECK_FALSE(received.has_value());
    deliver_completions();
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());
    CHECK(mv::settings().language.empty());
    CHECK(mv::settings().current_gardener_id == received->value().id);
    CHECK(mv::settings().current_collection_id == 0);
    REQUIRE(execute_on_worker(
        "CREATE TEMP TABLE verify_setup(value INTEGER CHECK(value=1));"
        "INSERT INTO verify_setup SELECT count(*)=1 FROM gardener;"
        "INSERT INTO verify_setup SELECT count(*)=0 FROM collection;"
        "INSERT INTO verify_setup SELECT language='' FROM settings;") == SQLITE_OK);
}

TEST_CASE("Atomic setup restores selection on restart") {
    const auto directory = std::filesystem::temp_directory_path() / std::format(
        "mainverte-selection-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    const auto utf8 = (directory / "database.db").u8string();
    const std::string filename(utf8.begin(), utf8.end());
    mv::id_t selected = 0;
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        CHECK(mv::settings().current_gardener_id == 0);
        std::optional<mv::GardenerResult> result;
        auto callback = [&](mv::GardenerResult value) { result = std::move(value); };
        REQUIRE(mv::complete_setup("Alice", "de", callback));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        CHECK(mv::settings().current_gardener_id == 0);
        deliver_completions();
        REQUIRE(result.has_value());
        REQUIRE(result->has_value());
        selected = result->value().id;
        CHECK(mv::settings().current_gardener_id == selected);
        CHECK(mv::settings().language == "de");
        CHECK(mv::settings().current_collection_id == 0);
        REQUIRE(execute_on_worker(
            "CREATE TRIGGER reject_selection BEFORE UPDATE ON settings BEGIN "
            "SELECT RAISE(ABORT, 'selection rejected'); END;") == SQLITE_OK);
        std::optional<bool> saved;
        REQUIRE(mv::save_settings({}, [&](bool success) { saved = success; }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(saved.has_value());
        CHECK_FALSE(*saved);
        CHECK(mv::settings().current_gardener_id == selected);
        CHECK(mv::settings().language == "de");
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        CHECK(mv::settings().current_gardener_id == selected);
        CHECK(mv::settings().language == "de");
        REQUIRE(execute_on_worker("DROP TRIGGER reject_selection; DELETE FROM gardener WHERE display_name='Alice'") == SQLITE_OK);
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        CHECK(mv::settings().current_gardener_id == 0);
        REQUIRE(execute_on_worker(
            "PRAGMA foreign_keys=OFF;"
            "UPDATE settings SET current_gardener_id=123456;") == SQLITE_OK);
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        CHECK(mv::settings().current_gardener_id == 0);
    }
}

TEST_CASE("Atomic setup rolls back failures and allows retry with another name") {
    const auto directory = std::filesystem::temp_directory_path() / std::format(
        "mainverte-setup-failure-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    const auto utf8 = (directory / "database.db").u8string();
    const std::string filename(utf8.begin(), utf8.end());
    std::optional<mv::Services> services;
    REQUIRE(services.emplace().initialize(filename));
    REQUIRE(execute_on_worker(
        "INSERT INTO gardener VALUES(42, 'Seed', 123);"
        "INSERT INTO collection VALUES(73, 42, 'Collection', 123, 123);") == SQLITE_OK);
    auto settings = mv::settings();
    settings.current_gardener_id = 42;
    settings.current_collection_id = 73;
    settings.language = "fr";
    REQUIRE(mv::save_settings(settings));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    deliver_completions();

    std::string stage;
    SUBCASE("Insertion failure") {
        stage = "insert";
        REQUIRE(execute_on_worker(
            "CREATE TRIGGER reject_operation BEFORE INSERT ON gardener BEGIN "
            "SELECT RAISE(ABORT, 'insertion rejected'); END;") == SQLITE_OK);
    }
    SUBCASE("Settings update failure") {
        stage = "write settings";
        REQUIRE(execute_on_worker(
            "CREATE TRIGGER reject_operation BEFORE UPDATE ON settings BEGIN "
            "SELECT RAISE(ABORT, 'selection rejected'); END;") == SQLITE_OK);
    }
    SUBCASE("Commit failure from a deferred foreign key") {
        stage = "commit";
        REQUIRE(execute_on_worker(
            "CREATE TABLE deferred_setup_check(gardener_id INTEGER REFERENCES gardener(id) "
            "DEFERRABLE INITIALLY DEFERRED);"
            "CREATE TRIGGER reject_operation AFTER UPDATE ON settings BEGIN "
            "INSERT INTO deferred_setup_check VALUES(999); END;") == SQLITE_OK);
    }
    SUBCASE("Missing settings row") {
        stage = "read settings";
        REQUIRE(execute_on_worker("DELETE FROM settings;") == SQLITE_OK);
    }

    std::optional<mv::GardenerResult> received;
    int calls = 0;
    const auto owner = std::this_thread::get_id();
    auto callback = [&](mv::GardenerResult result) {
        CHECK(std::this_thread::get_id() == owner);
        ++calls;
        if (result) {
            CHECK(mv::settings().current_gardener_id == result->id);
            CHECK(mv::settings().current_collection_id == 0);
        }
        received = std::move(result);
    };
    REQUIRE(mv::complete_setup("Alice", "de", callback));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    CHECK(calls == 0);
    deliver_completions();
    REQUIRE(received.has_value());
    REQUIRE_FALSE(received->has_value());
    CHECK(received->error().code == mv::MainverteError::Code::Database);
    INFO(received->error().message);
    CHECK(received->error().message.find("(" + stage + ")") != std::string::npos);
    CHECK(calls == 1);
    CHECK(mv::settings().current_gardener_id == 42);
    CHECK(mv::settings().current_collection_id == 73);
    CHECK(mv::settings().language == "fr");

    // Restore a missing settings row, then check rollback before allowing a retry.
    REQUIRE(execute_on_worker(
        "DROP TRIGGER IF EXISTS reject_operation;"
        "INSERT OR IGNORE INTO settings(id, current_gardener_id, current_collection_id, language) VALUES(1, 42, 73, 'fr');"
        "CREATE TRIGGER check_rollback BEFORE INSERT ON gardener BEGIN "
        "SELECT CASE WHEN (SELECT count(*) FROM gardener) != 1 "
        "OR NOT EXISTS(SELECT 1 FROM settings WHERE current_gardener_id=42 "
        "AND current_collection_id=73 AND language='fr') "
        "OR (SELECT count(*) FROM collection) != 1 "
        "THEN RAISE(ABORT, 'rollback incomplete') END; END;") == SQLITE_OK);
    services.reset();
    REQUIRE(services.emplace().initialize(filename));
    CHECK(mv::settings().current_gardener_id == 42);
    CHECK(mv::settings().current_collection_id == 73);
    CHECK(mv::settings().language == "fr");
    received.reset();
    const std::string name = "Bob'); DROP TABLE gardener; --";
    REQUIRE(mv::complete_setup(name, "es", callback));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    CHECK(mv::settings().current_gardener_id == 42);
    deliver_completions();
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());
    CHECK(received->value().name == name);
    CHECK(calls == 2);
    CHECK(mv::settings().language == "es");
    REQUIRE(execute_on_worker(
        "CREATE TEMP TABLE verify_setup(value INTEGER CHECK(value=1));"
        "INSERT INTO verify_setup SELECT count(*)=2 FROM gardener;"
        "INSERT INTO verify_setup SELECT count(*)=1 FROM collection;") == SQLITE_OK);
    services.reset();
    REQUIRE(services.emplace().initialize(filename));
    CHECK(mv::settings().current_gardener_id == received->value().id);
    CHECK(mv::settings().current_collection_id == 0);
    CHECK(mv::settings().language == "es");
}
