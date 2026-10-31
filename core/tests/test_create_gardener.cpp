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
#include "mv_definitions.h"
#include "mv_core.h"
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
    CHECK(mv::settings().current_gardener_id == 42);
    REQUIRE(mv::save_settings(settings));
    CHECK(mv::settings().current_gardener_id == 42);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    CHECK(mv::settings().current_gardener_id == 0);
    CHECK(mv::settings().current_collection_id == 73);
    REQUIRE(execute_on_worker(
        "CREATE TRIGGER check_settings BEFORE INSERT ON gardener BEGIN "
        "SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM settings WHERE current_gardener_id IS NULL "
        "AND current_collection_id=73) THEN RAISE(ABORT, 'settings not saved') END; END;") == SQLITE_OK);
    auto invalid_settings = mv::settings();
    invalid_settings.current_gardener_id = 42;
    invalid_settings.current_collection_id = 999;
    REQUIRE(mv::save_settings(invalid_settings));
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    CHECK(mv::settings().current_gardener_id == 0);
    CHECK(mv::settings().current_collection_id == 73);
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

TEST_CASE("Atomic gardener creation restores selection on restart") {
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
        REQUIRE(mv::create_and_select_gardener("Alice", callback));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        CHECK(mv::settings().current_gardener_id == 0);
        deliver_completions();
        REQUIRE(result.has_value());
        REQUIRE(result->has_value());
        selected = result->value().id;
        CHECK(mv::settings().current_gardener_id == selected);
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
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        CHECK(mv::settings().current_gardener_id == selected);
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

TEST_CASE("Atomic gardener creation rolls back failures and allows retry with another name") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
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

    SUBCASE("Insertion failure") {
        REQUIRE(execute_on_worker(
            "CREATE TRIGGER reject_operation BEFORE INSERT ON gardener BEGIN "
            "SELECT RAISE(ABORT, 'insertion rejected'); END;") == SQLITE_OK);
    }
    SUBCASE("Selection failure") {
        REQUIRE(execute_on_worker(
            "CREATE TRIGGER reject_operation BEFORE UPDATE ON settings BEGIN "
            "SELECT RAISE(ABORT, 'selection rejected'); END;") == SQLITE_OK);
    }
    SUBCASE("Commit failure from a deferred foreign key") {
        REQUIRE(execute_on_worker(
            "PRAGMA defer_foreign_keys=ON;"
            "CREATE TRIGGER reject_operation AFTER UPDATE ON settings BEGIN "
            "INSERT INTO collection VALUES(74, 999, 'Invalid', 123, 123); END;") == SQLITE_OK);
    }
    SUBCASE("Missing settings row") {
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
    REQUIRE(mv::create_and_select_gardener("Alice", callback));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    CHECK(calls == 0);
    deliver_completions();
    REQUIRE(received.has_value());
    REQUIRE_FALSE(received->has_value());
    CHECK(received->error().code == mv::MainverteError::Code::Database);
    CHECK_FALSE(received->error().message.empty());
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
        "AND current_collection_id=73) "
        "OR (SELECT count(*) FROM collection) != 1 "
        "THEN RAISE(ABORT, 'rollback incomplete') END; END;") == SQLITE_OK);
    received.reset();
    const std::string name = "Bob'); DROP TABLE gardener; --";
    REQUIRE(mv::create_and_select_gardener(name, callback));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    CHECK(mv::settings().current_gardener_id == 42);
    deliver_completions();
    REQUIRE(received.has_value());
    REQUIRE(received->has_value());
    CHECK(received->value().name == name);
    CHECK(calls == 2);
    CHECK(mv::settings().language == "fr");
}
