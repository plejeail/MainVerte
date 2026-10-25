#include "mainverte_core.h"

#include "database.h"
#include "services.h"
#include "tools.h"


std::unique_ptr<Database> service::database;


bool mv::Services::initialize(std::string_view database_path) {
    static bool initialized = false;
    assert(!initialized);
    if (initialized) {
        return true;
    }

    assert(service::database == nullptr);
    service::database = std::make_unique<Database>();
    if (!service::database->initialize(std::string(database_path))) {
        log_err("services: database initialization failed");
        service::database.reset();
        return false;
    }

    service::database->start();

    initialized = true;
    return true;
}

mv::Services::~Services() {
    service::database.reset();
}
