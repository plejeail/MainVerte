#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include <future>
#include <sqlite3.h>

#include "database.h"
#include "definitions.h"
#include "move_only_function.h"
#include "doctest.h"
#include "mainverte_core.h"
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
    for (auto& callback : pending) callback();
}

int execute_on_worker(const char* sql) {
    std::promise<int> completed;
    auto result = completed.get_future();
    if (!service::database().submit([sql, completed = std::move(completed)](sqlite3* handle) mutable {
            completed.set_value(sqlite3_exec(handle, sql, nullptr, nullptr, nullptr));
        })) return SQLITE_MISUSE;
    // Joining Services during stack unwinding also completes any accepted job.
    if (result.wait_for(std::chrono::seconds(5)) != std::future_status::ready) return SQLITE_BUSY;
    return result.get();
}
}

void mv::platform::dispatch_callback(MoveOnlyFunction<void()> callback) {
    std::lock_guard lock(completion_mutex);
    completions.push_back(std::move(callback));
}

TEST_CASE("create_gardener persists names and dispatches success and database errors") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
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
        "SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM gardener WHERE id = 1 "
        "AND display_name = 'MV''); DROP TABLE gardener; --' "
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

    REQUIRE(execute_on_worker("DROP TABLE gardener;") == SQLITE_OK);
    mv::create_gardener("Missing table", callback);
    REQUIRE(execute_on_worker("SELECT 1;") == SQLITE_OK);
    deliver_completions();
    REQUIRE_FALSE(received->has_value());
    CHECK(received->error().code == mv::MainverteError::Code::Database);
    CHECK(received->error().message.find("no such table") != std::string::npos);
    CHECK(calls == 4);
}
