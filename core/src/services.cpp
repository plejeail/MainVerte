#include "services.h"
#include "mainverte_core.h"

#include <cassert>
#include <expected>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <sqlite3.h>

#include "database.h"
#include "definitions.h"
#include "move_only_function.h"
#include "tools.h"


namespace {
struct ServicesImpl {
    Settings settings;
    Database database;
};

std::unique_ptr<ServicesImpl> services_instance;  // NOLINT
}  // namespace

Settings& service::settings() {
    assert(services_instance != nullptr);
    return services_instance->settings;
}

Database& service::database() {
    assert(services_instance != nullptr);
    return services_instance->database;
}

bool mv::Services::initialize(std::string_view database_path) {
    assert(!initialized);
    if (initialized) {
        return true;
    }

    initialized = true;

    assert(services_instance == nullptr);
    services_instance = std::make_unique<ServicesImpl>();

    auto& database = services_instance->database;
    if (!database.initialize(std::string(database_path))) {
        log_err("services: database initialization failed");
        return false;
    }

    auto settings = database.read_settings();
    if (!settings) {
        log_err("services: settings loading failed");
        return false;
    }
    services_instance->settings = *settings;
    database.start();

    return true;
}

mv::Services::~Services() {
    services_instance.reset();
}

mv::Settings mv::Services::settings() const {
    assert(services_instance != nullptr);
    return services_instance->settings;
}

bool mv::Services::save_settings(const Settings& settings) {
    assert(services_instance != nullptr);
    auto& database = services_instance->database;
    return database.submit([&database, settings](sqlite3*) {
        if (database.write_settings(settings)) {
            mv::platform::dispatch_callback([settings] {
                if (services_instance) {
                    services_instance->settings = settings;
                }
            });
        }
    });
}

bool mv::create_gardener(std::string name, MoveOnlyFunction<void(GardenerResult)> callback) {
    assert(services_instance != nullptr);
    assert(callback);
    if (name.size() == 0 || name.size() > 30) {
        return false;
    }

    // Retain the callback across submit(): rejected jobs destroy their captures.
    // Only the worker (accepted) or caller (rejected) consumes it.
    auto completion = std::make_shared<MoveOnlyFunction<void(GardenerResult)>>(std::move(callback));
    const bool accepted = service::database().submit(
    [name=std::move(name), completion](sqlite3* database) mutable {
        mv::id_t id = service::database().create_gardener(name);
        if (id >= 0) {
            dispatch_result(std::move(*completion), GardenerResult(Gardener{.id = id, .name = std::move(name)}));
        } else {
            auto message = std::format("create gardener failed: {}", sqlite3_errmsg(database));
            log_err(message);
            dispatch_result(std::move(*completion),
                GardenerResult(std::unexpected(
                    MainverteError{
                        .code = MainverteError::Code::Database,
                        .message = std::move(message)
                    }
                ))
            );
        }
    });

    if (!accepted) {
        dispatch_result(std::move(*completion), GardenerResult(std::unexpected(
            MainverteError{.code = MainverteError::Code::NotRunning, .message = "Database is not running"})));
    }

    return accepted;
}
