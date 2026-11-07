#include "mv_care.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

#include <sqlite3.h>

#include "mv_core.h"
#include "services.h"
#include "sql_query.h"

namespace {

mv::CareResult run(sqlite3* db, mv::id_t gardener, const mv::CareAction& action) {
    using K = mv::CareAction::Kind;
    using detail::Query;
    const auto fail = [&](std::string message, mv::MainverteError::Code code = mv::MainverteError::Code::Database) -> mv::CareResult {
        if (!sqlite3_get_autocommit(db) && sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
            message += "; rollback failed: ";
            message += sqlite3_errmsg(db);
        }

        mv::platform::log(mv::LogLevel::Wrn, message);
        return std::unexpected(mv::MainverteError{code, std::move(message)});
    };
    if (action.kind < K::List || action.kind > K::Delete ||
        (action.kind == K::Add && (action.type < mv::NeedType::Watering || action.type > mv::NeedType::Temperature))) {
        return fail("Unsupported care operation or need type", mv::MainverteError::Code::InvalidInput);
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK) return fail(sqlite3_errmsg(db));

    Query owner(db, "SELECT s.id FROM specimen s JOIN collection c ON c.id=s.collection_id WHERE s.id=? AND c.gardener_id=?");
    owner.bind(1, action.specimen_id);
    owner.bind(2, gardener);
    const int owner_rc = owner.step();
    if (owner_rc == SQLITE_DONE) return fail("Specimen unavailable", mv::MainverteError::Code::NotFound);

    if (owner_rc != SQLITE_ROW) return fail(sqlite3_errmsg(db));

    if (action.kind == K::Add) {
        Query insert(db, "INSERT INTO care_need(specimen_id,type) VALUES(?,?)");
        insert.bind(1, action.specimen_id);
        insert.bind(2, static_cast<int>(action.type));
        if (insert.step() != SQLITE_DONE) return fail(sqlite3_errmsg(db));

    } else if (action.kind == K::Delete) {
        Query remove(db, "DELETE FROM care_need WHERE id=? AND specimen_id=?");
        remove.bind(1, action.id);
        remove.bind(2, action.specimen_id);
        if (remove.step() != SQLITE_DONE) return fail(sqlite3_errmsg(db));

        if (sqlite3_changes(db) != 1) return fail("Care need unavailable", mv::MainverteError::Code::NotFound);

    }

    Query list(db, "SELECT id,specimen_id,type FROM care_need WHERE specimen_id=?1 AND (?2=0 OR id=?2) ORDER BY id");
    list.bind(1, action.specimen_id);
    list.bind(2, action.kind == K::Get ? action.id : 0);
    std::vector<mv::CareNeed> result;
    int rc;
    while ((rc = list.step()) == SQLITE_ROW) {
        result.push_back({list.id(0), list.id(1), static_cast<mv::NeedType>(list.id(2))});
    }

    if (rc != SQLITE_DONE) return fail(sqlite3_errmsg(db));

    if (action.kind == K::Get && (action.id <= 0 || result.empty())) return fail("Care need unavailable", mv::MainverteError::Code::NotFound);

    if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) return fail(sqlite3_errmsg(db));

    return result;
}

}  //  namespace

bool mv::manage_care(CareAction action, MoveOnlyFunction<void(CareResult)> callback) {
    assert(callback);
    auto done = std::make_shared<MoveOnlyFunction<void(CareResult)>>(std::move(callback));
    const auto gardener = settings().current_gardener_id;
    const bool accepted = service::database().submit([action, gardener, done](sqlite3* db) {
        auto result = run(db, gardener, action);
        platform::dispatch_callback([done, result = std::move(result)]() mutable { (*done)(std::move(result)); });
    });
    if (!accepted) {
        platform::dispatch_callback([done] { (*done)(std::unexpected(MainverteError{MainverteError::Code::NotRunning, "Database is not running"})); });
    }

    return accepted;
}
