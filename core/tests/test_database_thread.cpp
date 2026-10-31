// clazy:skip
#include <array>
#include <chrono>
#include <future>
#include <thread>
#include <utility>
#include <vector>

#include <doctest.h>
#include <sqlite3.h>

#include "database.h"


TEST_CASE("Database rejects jobs until started and rejects empty jobs") {
    Database db;
    CHECK_FALSE(db.submit([](sqlite3*) {}));
    REQUIRE(db.initialize(":memory:"));
    CHECK_FALSE(db.submit([](sqlite3*) {}));
    db.start();
    CHECK_FALSE(db.submit({}));
}

TEST_CASE("Database wakes repeatedly and runs SQLite jobs on one worker") {
    Database db;
    REQUIRE(db.initialize(":memory:"));
    db.start();
    const auto owner = std::this_thread::get_id();
    std::thread::id worker;
    for (int i = 0; i < 10; ++i) {
        std::promise<std::pair<std::thread::id, int>> completed;
        auto result = completed.get_future();
        REQUIRE(db.submit([completed = std::move(completed)](sqlite3* handle) mutable {
            completed.set_value({std::this_thread::get_id(),
                sqlite3_exec(handle, "CREATE TABLE IF NOT EXISTS plants(id INTEGER);",
                             nullptr, nullptr, nullptr)});
        }));
        REQUIRE(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        const auto [id, status] = result.get();
        CHECK(id != owner);
        CHECK(status == SQLITE_OK);
        if (i == 0) worker = id;
        CHECK(id == worker);
    }
}

TEST_CASE("Database drains pending jobs in FIFO order before destruction") {
    std::vector<int> executed;
    std::promise<void> release;
    auto ready = release.get_future();
    {
        Database db;
        struct ReleaseOnExit {
            std::promise<void>& promise;
            ~ReleaseOnExit() { promise.set_value(); }
        };
        REQUIRE(db.initialize(":memory:"));
        db.start();
        {
            // Release the worker even when a REQUIRE aborts this test scope.
            ReleaseOnExit release_worker{release};
            REQUIRE(db.submit([&](sqlite3*) { ready.wait(); }));
            for (int i = 0; i < 100; ++i) {
                REQUIRE(db.submit([&, i](sqlite3*) { executed.push_back(i); }));
            }
        }
    }
    REQUIRE(executed.size() == 100);
    for (int i = 0; i < 100; ++i) CHECK(executed[i] == i);
}

TEST_CASE("Database can stop immediately after starting without losing shutdown") {
    for (int i = 0; i < 100; ++i) {
        Database db;
        REQUIRE(db.initialize(":memory:"));
        db.start();
    }
}

TEST_CASE("Database jobs can enqueue more jobs without holding the queue lock") {
    Database db;
    REQUIRE(db.initialize(":memory:"));
    db.start();
    std::promise<bool> completed;
    auto result = completed.get_future();
    REQUIRE(db.submit([&](sqlite3*) {
        if (!db.submit([&](sqlite3*) { completed.set_value(true); })) {
            completed.set_value(false);
        }
    }));
    REQUIRE(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(result.get());
}

TEST_CASE("Concurrent producers execute every job once on one worker in producer order") {
    constexpr int producer_count = 4;
    constexpr int jobs_per_producer = 100;
    struct Executed {
        int producer;
        int sequence;
        std::thread::id thread;
    };
    std::vector<Executed> executed;
    std::array<int, producer_count> accepted{};
    {
        Database db;
        REQUIRE(db.initialize(":memory:"));
        db.start();
        std::array<std::thread, producer_count> producers;
        for (int producer = 0; producer < producer_count; ++producer) {
            producers[producer] = std::thread([&, producer] {
                for (int sequence = 0; sequence < jobs_per_producer; ++sequence) {
                    if (db.submit([&, producer, sequence](sqlite3*) {
                        executed.push_back({producer, sequence, std::this_thread::get_id()});
                    })) ++accepted[producer];
                }
            });
        }
        for (auto& producer : producers) producer.join();
        // The database drains accepted jobs before the results are inspected.
    }
    REQUIRE(executed.size() == producer_count * jobs_per_producer);
    std::array<int, producer_count> next{};
    const auto worker = executed.front().thread;
    CHECK(worker != std::this_thread::get_id());
    for (const auto& job : executed) {
        CHECK(job.thread == worker);
        CHECK(job.sequence == next[job.producer]++);
    }
    for (int producer = 0; producer < producer_count; ++producer) {
        CHECK(accepted[producer] == jobs_per_producer);
        CHECK(next[producer] == jobs_per_producer);
    }
}
