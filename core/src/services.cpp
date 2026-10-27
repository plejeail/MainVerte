#include "services.h"
#include "mainverte_core.h"

#include "database.h"
#include "move_only_function.h"
#include "settings.h"
#include "tools.h"

#include <sqlite3.h>


namespace {
struct ServicesImpl {
    Settings settings;
    Database database;
};

std::unique_ptr<ServicesImpl> services_instance;  // NOLINT
}  // namespace

Settings& service::settings() {
    assert(services_instance);
    return services_instance->settings;
}

Database& service::database() {
    assert(services_instance);
    return services_instance->database;
}

bool mv::Services::initialize(std::string_view database_path) {
    static bool initialized = false;
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

    database.start();

    return true;
}

mv::Services::~Services() {
    services_instance.reset();
}

void mv::create_gardener(std::string name, MoveOnlyFunction<void(CreateGardenerResult)> callback) {
    assert(callback);
    // Retain the callback across submit(): rejected jobs destroy their captures.
    // Only the worker (accepted) or caller (rejected) consumes it.
    auto completion = std::make_shared<MoveOnlyFunction<void(CreateGardenerResult)>>(std::move(callback));
    const bool accepted = service::database().submit(
    [name=std::move(name), completion](sqlite3* database) mutable {
        mv::id_t id = service::database().create_gardener(name);
        if (id >= 0) {
            dispatch_result(std::move(*completion), CreateGardenerResult(Gardener{.id = id, .name = std::move(name)}));
        } else {
            auto message = std::format("create gardener failed: {}", sqlite3_errmsg(database));
            log_err(message);
            dispatch_result(std::move(*completion),
                CreateGardenerResult(std::unexpected(
                    MainverteError{
                        .code = MainverteError::Code::Database,
                        .message = std::move(message)
                    }
                ))
            );
        }
    });

    if (!accepted) {
        dispatch_result(std::move(*completion), CreateGardenerResult(std::unexpected(
            MainverteError{.code = MainverteError::Code::NotRunning, .message = "Database is not running"})));
    }
}
