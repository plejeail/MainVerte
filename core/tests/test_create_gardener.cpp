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

}  //  namespace

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

TEST_CASE("plant workflow preserves identities ownership and restart") {
    const auto directory = std::filesystem::temp_directory_path() / std::format(
        "mainverte-plants-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    const auto filename = (directory / "test.db").string();
    mv::id_t plant_id = 0;
    mv::id_t destination = 0;
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        bool created = false;
        REQUIRE(mv::complete_setup("Gardener", "fr", [&](mv::GardenerResult r) { created = r.has_value(); }, "My Plants"));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(created);
        REQUIRE(mv::settings().current_collection_id > 0);
        CHECK(mv::settings().language == "fr");
        const auto invoke = [&](mv::PlantAction action) {
            std::optional<mv::PlantsResult> result;
            REQUIRE(mv::manage_plants(std::move(action), "My Plants", [&](mv::PlantsResult r) { result = std::move(r); }));
            REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
            deliver_completions();
            REQUIRE(result.has_value());
            return std::move(*result);
        };
        using K = mv::PlantAction::Kind;
        auto state = invoke({});
        REQUIRE(state);
        REQUIRE(state->collections.size() == 1);
        const auto initial = state->selected;
        const auto last_collection = invoke({.kind=K::DeleteCollection, .id=initial});
        REQUIRE_FALSE(last_collection);
        CHECK(last_collection.error().code == mv::MainverteError::Code::LastCollection);
        state = invoke({});
        REQUIRE(state);
        CHECK(state->collections.size() == 1);
        CHECK(state->selected == initial);
        CHECK_FALSE(invoke({.kind=K::CreateCollection, .name="  "}));
        state = invoke({.kind=K::CreateCollection, .name="Cuttings"});
        REQUIRE(state);
        REQUIRE(state->collections.size() == 2);
        destination = state->collections.back().id;
        CHECK(invoke({.kind=K::RenameCollection, .id=destination, .name="New cuttings"}));
        CHECK_FALSE(invoke({.kind=K::SelectCollection, .id=9999}));
        CHECK_FALSE(invoke({.kind=K::CreateSpecimen, .id=initial, .name="\t"}));
        state = invoke({.kind=K::CreateSpecimen, .id=initial, .name="Nishiro", .botanical_name="Gymnocalycium"});
        REQUIRE(state);
        REQUIRE(state->specimens.size() == 1);
        plant_id = state->specimens.front().id;
        CHECK(invoke({.kind=K::CreateSpecimen, .id=initial, .name="Nishiro"}));
        CHECK_FALSE(invoke({.kind=K::DeleteCollection, .id=initial}));
        CHECK(invoke({.kind=K::UpdateSpecimen, .id=plant_id, .name="Nishiro", .cultivar="Updated"}));
        CHECK_FALSE(invoke({.kind=K::MoveSpecimen, .id=plant_id, .destination=9999}));
        CHECK(invoke({.kind=K::MoveSpecimen, .id=plant_id, .destination=destination}));
        auto appearance = mv::settings();
        appearance.dark_theme_enabled = !appearance.dark_theme_enabled;
        REQUIRE(mv::manage_plants({.kind=K::SelectCollection, .id=destination}, "My Plants", [](mv::PlantsResult r) { CHECK(r); }));
        REQUIRE(mv::save_settings(appearance));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        CHECK(mv::settings().current_collection_id == destination);
        state = invoke({});
        REQUIRE(state);
        REQUIRE(state->specimens.size() == 1);
        CHECK(state->specimens.front().cultivar == "Updated");
        state = invoke({.kind=K::ArchiveSpecimen, .id=plant_id});
        REQUIRE(state);
        CHECK(state->specimens.front().archived);
        CHECK_FALSE(invoke({.kind=K::DeleteCollection, .id=destination}));
        state = invoke({.kind=K::RestoreSpecimen, .id=plant_id});
        REQUIRE(state);
        CHECK_FALSE(state->specimens.front().archived);
        state = invoke({.kind=K::CreateCollection, .name="Empty"});
        REQUIRE(state);
        CHECK(invoke({.kind=K::DeleteCollection, .id=state->collections.back().id}));
        REQUIRE(execute_on_worker("INSERT INTO gardener VALUES(999,'Other',1); INSERT INTO collection VALUES(999,999,'Private',1,1); INSERT INTO specimen(id,collection_id,display_name,created_at,modified_at) VALUES(999,999,'Private plant',1,1)") == SQLITE_OK);
        CHECK_FALSE(invoke({.kind=K::SelectCollection, .id=999}));
        CHECK_FALSE(invoke({.kind=K::RenameCollection, .id=999, .name="Forbidden"}));
        CHECK_FALSE(invoke({.kind=K::DeleteCollection, .id=999}));
        CHECK_FALSE(invoke({.kind=K::CreateSpecimen, .id=999, .name="Forbidden"}));
        CHECK_FALSE(invoke({.kind=K::ListSpecimens, .id=999}));
        CHECK_FALSE(invoke({.kind=K::MoveSpecimen, .id=plant_id, .destination=999}));
        CHECK_FALSE(invoke({.kind=K::GetSpecimen, .id=999}));
        state = invoke({.kind=K::GetSpecimen, .id=plant_id});
        REQUIRE(state);
        REQUIRE(state->specimens.size() == 1);
        CHECK(state->specimens.front().id == plant_id);
        CHECK(state->specimens.front().created_at > 0);
        state = invoke({.kind=K::ListSpecimens, .id=initial});
        REQUIRE(state);
        CHECK(state->specimens.size() == 1);
        CHECK(state->selected == destination);
        REQUIRE(execute_on_worker("CREATE TRIGGER fail_specimen BEFORE UPDATE ON specimen BEGIN SELECT RAISE(ABORT,'forced failure'); END") == SQLITE_OK);
        CHECK_FALSE(invoke({.kind=K::ArchiveSpecimen, .id=plant_id}));
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(filename));
        CHECK(mv::settings().current_collection_id == destination);
        std::optional<mv::PlantsResult> result;
        REQUIRE(mv::manage_plants({}, "My Plants", [&](mv::PlantsResult r) { result = std::move(r); }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(result);
        REQUIRE(*result);
        REQUIRE((*result)->specimens.size() == 1);
        CHECK((*result)->specimens.front().id == plant_id);
        CHECK_FALSE((*result)->specimens.front().archived);
    }
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    CHECK_FALSE(error);
}

TEST_CASE("legacy gardener collection initialization is idempotent and setup rolls back") {
    const auto directory = std::filesystem::temp_directory_path() / std::format(
        "mainverte-legacy-plants-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = (directory / "legacy.db").string();
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        REQUIRE(execute_on_worker("INSERT INTO gardener VALUES(42,'Legacy',1); UPDATE settings SET current_gardener_id=42") == SQLITE_OK);
        // Load the owner-thread cache through the existing settings operation.
        auto settings = mv::settings();
        settings.current_gardener_id = 42;
        REQUIRE(mv::save_settings(settings));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        mv::id_t selected = 0;
        for (int i = 0; i < 3; ++i) {
            std::optional<mv::PlantsResult> result;
            REQUIRE(mv::manage_plants({}, "Mes plantes", [&](mv::PlantsResult r) { result = std::move(r); }));
            REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
            deliver_completions();
            REQUIRE(result);
            REQUIRE(*result);
            REQUIRE((*result)->collections.size() == 1);
            CHECK((*result)->collections.front().name == "Mes plantes");
            if (i == 0) selected = (*result)->selected;
            CHECK((*result)->selected == selected);
        }

        REQUIRE(execute_on_worker("UPDATE settings SET current_collection_id=NULL") == SQLITE_OK);
        std::optional<mv::PlantsResult> repaired;
        REQUIRE(mv::manage_plants({}, "Another name", [&](mv::PlantsResult r) { repaired = std::move(r); }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(repaired);
        REQUIRE(*repaired);
        CHECK((*repaired)->collections.size() == 1);
        CHECK((*repaired)->selected == selected);
        REQUIRE(execute_on_worker("CREATE TRIGGER reject_collection BEFORE INSERT ON collection BEGIN SELECT RAISE(ABORT,'forced'); END") == SQLITE_OK);
        bool failed = false;
        REQUIRE(mv::complete_setup("Failed gardener", "de", [&](mv::GardenerResult r) { failed = !r; }, "My Plants"));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        CHECK(failed);
        CHECK(mv::settings().current_gardener_id == 42);
        CHECK(mv::settings().language.empty());
        REQUIRE(execute_on_worker("CREATE TRIGGER verify_rollback BEFORE INSERT ON gardener WHEN (SELECT count(*) FROM gardener)<>1 BEGIN SELECT RAISE(ABORT,'partial setup'); END; INSERT INTO gardener VALUES(88,'Rollback verified',1)") == SQLITE_OK);
    }
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    CHECK_FALSE(error);
}

TEST_CASE("Specimen details and independent care needs persist across moves archives and restart") {
    const auto directory = std::filesystem::temp_directory_path() / std::format(
        "mainverte-care-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    const auto path = (directory / "care.db").string();
    const auto plant = [&](mv::PlantAction action) {
        std::optional<mv::PlantsResult> result;
        REQUIRE(mv::manage_plants(std::move(action), "My Plants", [&](mv::PlantsResult value) { result = std::move(value); }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(result);
        return std::move(*result);
    };
    const auto care = [&](mv::CareAction action) {
        std::optional<mv::CareResult> result;
        const auto thread = std::this_thread::get_id();
        int calls = 0;
        REQUIRE(mv::manage_care(action, [&](mv::CareResult value) {
            CHECK(std::this_thread::get_id() == thread);
            ++calls;
            result = std::move(value);
        }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        CHECK_FALSE(result);
        deliver_completions();
        REQUIRE(result);
        CHECK(calls == 1);
        return std::move(*result);
    };
    using P = mv::PlantAction::Kind;
    using C = mv::CareAction::Kind;
    mv::id_t specimen = 0, other = 0, watering = 0, repotting = 0;
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        REQUIRE(mv::complete_setup("Camille", "fr", [](mv::GardenerResult value) { REQUIRE(value); }, "My Plants"));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        const auto collection = mv::settings().current_collection_id;
        auto state = plant({.kind=P::CreateSpecimen, .id=collection, .name="Nishiro", .botanical_name="Gymnocalycium", .cultivar="Mihanovichii",
                            .profile=mv::PlantProfile::Succulent, .acquired_on="2024-02-29"});
        REQUIRE(state);
        specimen = state->specimens.front().id;
        auto needs = care({.specimen_id=specimen});
        REQUIRE(needs);
        CHECK(needs->empty());
        needs = care({.kind=C::Add, .specimen_id=specimen, .type=mv::NeedType::Watering});
        REQUIRE(needs);
        REQUIRE(needs->size() == 1);
        watering = needs->front().id;
        needs = care({.kind=C::Add, .specimen_id=specimen, .type=mv::NeedType::Repotting});
        REQUIRE(needs);
        REQUIRE(needs->size() == 2);
        repotting = needs->back().id;
        CHECK(watering != repotting);
        auto retrieved = care({.kind=C::Get, .specimen_id=specimen, .id=watering});
        REQUIRE(retrieved);
        REQUIRE(retrieved->size() == 1);
        CHECK(retrieved->front().type == mv::NeedType::Watering);
        CHECK(retrieved->front().specimen_id == specimen);
        // Repeated types have independent IDs and no Core uniqueness invariant.
        needs = care({.kind=C::Add, .specimen_id=specimen, .type=mv::NeedType::Watering});
        REQUIRE(needs);
        REQUIRE(needs->size() == 3);
        const auto duplicate = needs->back().id;
        CHECK(duplicate != watering);
        REQUIRE(care({.kind=C::Delete, .specimen_id=specimen, .id=duplicate}));
        CHECK_FALSE(care({.kind=C::Get, .specimen_id=specimen, .id=duplicate}));
        CHECK_FALSE(care({.kind=C::Delete, .specimen_id=specimen, .id=duplicate}));
        state = plant({.kind=P::CreateSpecimen, .id=collection, .name="Other"});
        REQUIRE(state);
        other = state->specimens.front().id;
        CHECK_FALSE(state->specimens.front().profile);
        CHECK_FALSE(state->specimens.front().acquired_on);
        REQUIRE(care({.kind=C::Add, .specimen_id=other, .type=mv::NeedType::Light}));
        CHECK_FALSE(care({.kind=C::Get, .specimen_id=other, .id=watering}));
        CHECK_FALSE(care({.kind=C::Delete, .specimen_id=other, .id=watering}));
        CHECK_FALSE(care({.kind=C::Add, .specimen_id=99999}));
        CHECK_FALSE(care({.kind=C::Add, .specimen_id=specimen, .type=static_cast<mv::NeedType>(99)}));
        CHECK_FALSE(plant({.kind=P::UpdateSpecimen, .id=specimen, .name="Nishiro", .acquired_on="2025-02-29"}));
        CHECK_FALSE(plant({.kind=P::UpdateSpecimen, .id=specimen, .name="Nishiro", .acquired_on="2024-13-01"}));
        CHECK_FALSE(plant({.kind=P::UpdateSpecimen, .id=specimen, .name="Nishiro", .acquired_on="2024-1-01"}));
        CHECK_FALSE(plant({.kind=P::UpdateSpecimen, .id=specimen, .name="Nishiro", .profile=static_cast<mv::PlantProfile>(42)}));
        state = plant({.kind=P::CreateCollection, .name="Sunroom"});
        REQUIRE(state);
        REQUIRE(plant({.kind=P::MoveSpecimen, .id=specimen, .destination=state->collections.back().id}));
        REQUIRE(plant({.kind=P::ArchiveSpecimen, .id=specimen}));
        needs = care({.specimen_id=specimen});
        REQUIRE(needs);
        CHECK(needs->size() == 2);
        // Database errors do not report success or erase the previously committed list.
        REQUIRE(execute_on_worker("CREATE TRIGGER fail_care BEFORE INSERT ON care_need BEGIN SELECT RAISE(ABORT,'forced care failure'); END") == SQLITE_OK);
        const auto failed = care({.kind=C::Add, .specimen_id=specimen, .type=mv::NeedType::Pruning});
        REQUIRE_FALSE(failed);
        CHECK(failed.error().message.find("forced care failure") != std::string::npos);
        REQUIRE(execute_on_worker("DROP TRIGGER fail_care") == SQLITE_OK);
        REQUIRE(execute_on_worker("INSERT INTO gardener VALUES(999,'Other gardener',1); INSERT INTO collection VALUES(999,999,'Private',1,1); INSERT INTO specimen(id,collection_id,display_name,created_at,modified_at) VALUES(999,999,'Private',1,1); INSERT INTO care_need(id,specimen_id,type) VALUES(999,999,0)") == SQLITE_OK);
        CHECK_FALSE(care({.specimen_id=999}));
        CHECK_FALSE(care({.kind=C::Add, .specimen_id=999}));
        CHECK_FALSE(care({.kind=C::Delete, .specimen_id=999, .id=999}));
        CHECK_FALSE(care({.kind=C::Get, .specimen_id=999, .id=999}));
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        auto state = plant({.kind=P::GetSpecimen, .id=specimen});
        REQUIRE(state);
        REQUIRE(state->specimens.size() == 1);
        const auto& value = state->specimens.front();
        CHECK(value.display_name == "Nishiro");
        CHECK(value.botanical_name == "Gymnocalycium");
        CHECK(value.cultivar == "Mihanovichii");
        CHECK(value.profile == mv::PlantProfile::Succulent);
        CHECK(value.acquired_on == "2024-02-29");
        CHECK(value.archived);
        auto needs = care({.specimen_id=specimen});
        REQUIRE(needs);
        REQUIRE(needs->size() == 2);
        CHECK(needs->front().id == watering);
        CHECK(needs->back().id == repotting);
        needs = care({.kind=C::Delete, .specimen_id=specimen, .id=watering});
        REQUIRE(needs);
        REQUIRE(needs->size() == 1);
        CHECK(needs->front().id == repotting);
        CHECK(needs->front().type == mv::NeedType::Repotting);
        REQUIRE(care({.kind=C::Add, .specimen_id=specimen, .type=mv::NeedType::Watering}));
        needs = care({.specimen_id=other});
        REQUIRE(needs);
        REQUIRE(needs->size() == 1);
        CHECK(needs->front().type == mv::NeedType::Light);
        REQUIRE(plant({.kind=P::RestoreSpecimen, .id=specimen}));
        REQUIRE(plant({.kind=P::UpdateSpecimen, .id=specimen, .name="Renamed Nishiro"}));
        state = plant({.kind=P::GetSpecimen, .id=specimen});
        REQUIRE(state);
        CHECK(state->specimens.front().display_name == "Renamed Nishiro");
        CHECK(state->specimens.front().profile == mv::PlantProfile::Succulent);
        CHECK(state->specimens.front().acquired_on == "2024-02-29");
        REQUIRE(plant({.kind=P::UpdateSpecimen, .id=specimen, .name="Renamed Nishiro", .replace_optional_metadata=true}));
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        auto state = plant({.kind=P::GetSpecimen, .id=specimen});
        REQUIRE(state);
        CHECK(state->specimens.front().display_name == "Renamed Nishiro");
        CHECK_FALSE(state->specimens.front().profile);
        CHECK_FALSE(state->specimens.front().acquired_on);
    }
}

TEST_CASE("Plant queries own text and preserve nullable metadata across repeated operations") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    REQUIRE(mv::complete_setup("Gardener", "fr", [](mv::GardenerResult result) { REQUIRE(result); }, "Plants"));
    REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
    deliver_completions();

    const auto invoke = [](mv::PlantAction action) {
        std::optional<mv::PlantsResult> result;
        REQUIRE(mv::manage_plants(std::move(action), "Plants", [&](mv::PlantsResult value) { result = std::move(value); }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(result.has_value());
        return std::move(*result);
    };
    using K = mv::PlantAction::Kind;
    const auto collection = mv::settings().current_collection_id;
    auto created = invoke({.kind=K::CreateSpecimen, .id=collection, .name="First",
                           .botanical_name="Botanical", .cultivar="Cultivar",
                           .profile=mv::PlantProfile::Herb, .acquired_on="2024-02-29"});
    REQUIRE(created);
    REQUIRE(created->specimens.size() == 1);
    const auto id = created->specimens.front().id;
    REQUIRE(invoke({.kind=K::CreateSpecimen, .id=collection, .name="Second"}));
    // Text in the returned result survives finalization and later statements.
    CHECK(created->specimens.front().display_name == "First");
    CHECK(created->specimens.front().botanical_name == "Botanical");
    CHECK(created->specimens.front().cultivar == "Cultivar");
    CHECK(created->specimens.front().profile == mv::PlantProfile::Herb);
    CHECK(created->specimens.front().acquired_on == "2024-02-29");
    CHECK_FALSE(created->specimens.front().archived_at);

    REQUIRE(invoke({.kind=K::UpdateSpecimen, .id=id, .name="Renamed"}));
    auto state = invoke({.kind=K::GetSpecimen, .id=id});
    REQUIRE(state);
    REQUIRE(state->specimens.size() == 1);
    CHECK(state->specimens.front().profile == mv::PlantProfile::Herb);
    CHECK(state->specimens.front().acquired_on == "2024-02-29");
    REQUIRE(invoke({.kind=K::UpdateSpecimen, .id=id, .name="Renamed", .replace_optional_metadata=true}));
    state = invoke({.kind=K::ArchiveSpecimen, .id=id});
    REQUIRE(state);
    state = invoke({.kind=K::GetSpecimen, .id=id});
    REQUIRE(state);
    CHECK(state->specimens.front().archived);
    CHECK(state->specimens.front().archived_at.has_value());
    CHECK_FALSE(state->specimens.front().profile);
    CHECK_FALSE(state->specimens.front().acquired_on);

    REQUIRE(execute_on_worker("CREATE TRIGGER reject_plant BEFORE UPDATE ON specimen BEGIN SELECT RAISE(ABORT,'plant rejected'); END") == SQLITE_OK);
    const auto failed = invoke({.kind=K::RestoreSpecimen, .id=id});
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code == mv::MainverteError::Code::Database);
    CHECK(failed.error().message.find("plant rejected") != std::string::npos);
    REQUIRE(execute_on_worker("DROP TRIGGER reject_plant") == SQLITE_OK);
    REQUIRE(invoke({.kind=K::RestoreSpecimen, .id=id}));
    state = invoke({.kind=K::GetSpecimen, .id=id});
    REQUIRE(state);
    CHECK_FALSE(state->specimens.front().archived);
    CHECK_FALSE(state->specimens.front().archived_at);
}

namespace {

struct CareFixture {
    // Declared first so the database is closed before its directory is removed.
    struct Directory {
        std::filesystem::path path = std::filesystem::temp_directory_path() / std::format(
            "mainverte-care-{}", std::chrono::steady_clock::now().time_since_epoch().count());
        ~Directory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } directory;
    mv::Services services;

    CareFixture() {
        REQUIRE(services.initialize((directory.path / "test.db").string()));
        bool created = false;
        REQUIRE(mv::complete_setup("Gardener", "fr", [&](mv::GardenerResult r) { created = r.has_value(); }, "My Plants"));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(created);
    }

    mv::Plants plants(mv::PlantAction action) {
        std::optional<mv::PlantsResult> result;
        REQUIRE(mv::manage_plants(std::move(action), "My Plants", [&](mv::PlantsResult r) { result = std::move(r); }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(result.has_value());
        REQUIRE(result->has_value());
        return std::move(**result);
    }

    mv::CareResult care(mv::CareAction action) {
        std::optional<mv::CareResult> result;
        REQUIRE(mv::manage_care(action, [&](mv::CareResult r) { result = std::move(r); }));
        REQUIRE(execute_on_worker("SELECT 1") == SQLITE_OK);
        deliver_completions();
        REQUIRE(result.has_value());
        return std::move(*result);
    }

    mv::id_t create_specimen(const char* name) {
        using K = mv::PlantAction::Kind;
        return plants({.kind = K::CreateSpecimen, .id = mv::settings().current_collection_id, .name = name}).specimens.front().id;
    }
};

}  //  namespace

TEST_CASE("care needs belong to their own specimen across repeated operations") {
    CareFixture fixture;
    using C = mv::CareAction::Kind;
    const auto alpha = fixture.create_specimen("Alpha");
    const auto beta = fixture.create_specimen("Beta");

    auto added_alpha = fixture.care({.kind = C::Add, .specimen_id = alpha, .type = mv::NeedType::Watering});
    REQUIRE(added_alpha);
    REQUIRE(added_alpha->size() == 1);
    const auto alpha_need = added_alpha->front().id;

    auto added_beta = fixture.care({.kind = C::Add, .specimen_id = beta, .type = mv::NeedType::Light});
    REQUIRE(added_beta);
    REQUIRE(added_beta->size() == 1);
    CHECK(added_beta->front().specimen_id == beta);
    const auto beta_light = added_beta->front().id;

    auto list_alpha = fixture.care({.kind = C::List, .specimen_id = alpha});
    REQUIRE(list_alpha);
    REQUIRE(list_alpha->size() == 1);
    CHECK(list_alpha->front().specimen_id == alpha);
    CHECK(list_alpha->front().type == mv::NeedType::Watering);

    auto list_beta = fixture.care({.kind = C::List, .specimen_id = beta});
    REQUIRE(list_beta);
    REQUIRE(list_beta->size() == 1);
    CHECK(list_beta->front().specimen_id == beta);
    CHECK(list_beta->front().type == mv::NeedType::Light);

    // Repeated types are allowed per specimen, and each delete targets only its own need.
    auto duplicate = fixture.care({.kind = C::Add, .specimen_id = beta, .type = mv::NeedType::Watering});
    REQUIRE(duplicate);
    REQUIRE(duplicate->size() == 2);
    const auto beta_watering = duplicate->back().id;

    auto removed_alpha = fixture.care({.kind = C::Delete, .specimen_id = alpha, .id = alpha_need});
    REQUIRE(removed_alpha);
    CHECK(removed_alpha->empty());

    auto removed_beta = fixture.care({.kind = C::Delete, .specimen_id = beta, .id = beta_watering});
    REQUIRE(removed_beta);
    REQUIRE(removed_beta->size() == 1);
    CHECK(removed_beta->front().id == beta_light);

    auto got = fixture.care({.kind = C::Get, .specimen_id = beta, .id = beta_light});
    REQUIRE(got);
    REQUIRE(got->size() == 1);
    CHECK(got->front().type == mv::NeedType::Light);
}

TEST_CASE("refused and failed care operations leave later operations usable") {
    CareFixture fixture;
    using C = mv::CareAction::Kind;
    const auto alpha = fixture.create_specimen("Alpha");

    auto missing = fixture.care({.kind = C::Delete, .specimen_id = alpha, .id = 9999});
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == mv::MainverteError::Code::NotFound);

    auto listed = fixture.care({.kind = C::List, .specimen_id = alpha});
    REQUIRE(listed);
    CHECK(listed->empty());

    REQUIRE(execute_on_worker(
        "CREATE TRIGGER block_humidity BEFORE INSERT ON care_need WHEN NEW.type=6 "
        "BEGIN SELECT RAISE(ABORT, 'blocked'); END;") == SQLITE_OK);
    auto blocked = fixture.care({.kind = C::Add, .specimen_id = alpha, .type = mv::NeedType::Humidity});
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().code == mv::MainverteError::Code::Database);
    REQUIRE(execute_on_worker("DROP TRIGGER block_humidity") == SQLITE_OK);

    // A different type exposes any values left over from the refused insert.
    auto recovered = fixture.care({.kind = C::Add, .specimen_id = alpha, .type = mv::NeedType::Light});
    REQUIRE(recovered);
    REQUIRE(recovered->size() == 1);
    CHECK(recovered->front().type == mv::NeedType::Light);
}

TEST_CASE("care operations are limited to specimens of the current gardener") {
    CareFixture fixture;
    using C = mv::CareAction::Kind;
    fixture.create_specimen("Alpha");
    REQUIRE(execute_on_worker(
        "INSERT INTO gardener VALUES(999,'Other',1);"
        "INSERT INTO collection VALUES(999,999,'Private',1,1);"
        "INSERT INTO specimen(id,collection_id,display_name,created_at,modified_at) VALUES(999,999,'Private plant',1,1);"
        "INSERT INTO care_need(id,specimen_id,type) VALUES(900,999,0);") == SQLITE_OK);

    auto listed = fixture.care({.kind = C::List, .specimen_id = 999});
    REQUIRE_FALSE(listed);
    CHECK(listed.error().code == mv::MainverteError::Code::NotFound);

    auto added = fixture.care({.kind = C::Add, .specimen_id = 999, .type = mv::NeedType::Light});
    REQUIRE_FALSE(added);
    CHECK(added.error().code == mv::MainverteError::Code::NotFound);

    auto removed = fixture.care({.kind = C::Delete, .specimen_id = 999, .id = 900});
    REQUIRE_FALSE(removed);
    CHECK(removed.error().code == mv::MainverteError::Code::NotFound);

    REQUIRE(execute_on_worker(
        "CREATE TEMP TABLE verify_scope(value INTEGER CHECK(value=1));"
        "INSERT INTO verify_scope SELECT count(*)=1 FROM care_need WHERE id=900 AND specimen_id=999 AND type=0;"
        "INSERT INTO verify_scope SELECT count(*)=1 FROM care_need WHERE specimen_id=999;") == SQLITE_OK);
}
