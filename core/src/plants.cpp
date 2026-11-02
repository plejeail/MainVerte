#include "mv_plants.h"

#include <cassert>
#include <memory>
#include <utility>

#include <sqlite3.h>

#include "database.h"
#include "mv_core.h"
#include "services.h"

namespace {
using Statement = Database::statement_ptr;
struct Query {
    sqlite3* db;
    Statement stmt{nullptr, sqlite3_finalize};
    bool ok = false;
    Query(sqlite3* handle, const char* sql) : db(handle) {
        sqlite3_stmt* raw = nullptr;
        ok = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) == SQLITE_OK;
        stmt.reset(raw);
    }
    void bind(int n, mv::id_t value) { ok = ok && sqlite3_bind_int64(stmt.get(), n, value) == SQLITE_OK; }
    void bind(int n, const std::string& value) {
        ok = ok && sqlite3_bind_text64(stmt.get(), n, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8) == SQLITE_OK;
    }
    int step() { return ok ? sqlite3_step(stmt.get()) : SQLITE_ERROR; }
    std::string text(int col) {
        const auto* value = sqlite3_column_text(stmt.get(), col);
        return value ? std::string(reinterpret_cast<const char*>(value), static_cast<size_t>(sqlite3_column_bytes(stmt.get(), col))) : std::string{};
    }
    mv::id_t id(int col) { return sqlite3_column_int64(stmt.get(), col); }
};
bool valid(const std::string& name) { return name.find('\0') == std::string::npos && name.find_first_not_of(" \t\r\n") != std::string::npos; }

mv::PlantsResult run(sqlite3* db, mv::id_t gardener, const mv::PlantAction& a, const std::string& initial) {
    using K = mv::PlantAction::Kind;
    const auto fail = [&](std::string message, mv::MainverteError::Code code = mv::MainverteError::Code::Database) -> mv::PlantsResult {
        if (!sqlite3_get_autocommit(db) && sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
            message += "; rollback failed: ";
            message += sqlite3_errmsg(db);
        }

        mv::platform::log(mv::LogLevel::Wrn, message);
        return std::unexpected(mv::MainverteError{code, std::move(message)});
    };
    if (gardener <= 0 || sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK) {
        return fail("Unable to open plant operation");
    }

    Query owner(db, "SELECT id FROM gardener WHERE id=?");
    owner.bind(1, gardener);
    if (owner.step() != SQLITE_ROW) {
        return fail("Gardener does not exist");
    }

    Query count(db, "SELECT id FROM collection WHERE gardener_id=? ORDER BY id LIMIT 1");
    count.bind(1, gardener);
    const int count_rc = count.step();
    if (count_rc == SQLITE_DONE && a.kind != K::CreateCollection) {
        Query insert(db, "INSERT INTO collection(gardener_id,name,created_at,modified_at) VALUES(?,?,unixepoch(),unixepoch())");
        insert.bind(1, gardener);
        insert.bind(2, initial);
        if (!valid(initial) || insert.step() != SQLITE_DONE) {
            return fail("Unable to create initial collection");
        }
    } else if (count_rc != SQLITE_ROW && count_rc != SQLITE_DONE) {
        return fail(sqlite3_errmsg(db));
    }

    Query repair(db, "UPDATE settings SET current_collection_id=(SELECT id FROM collection WHERE gardener_id=? ORDER BY id LIMIT 1) WHERE id=1 AND current_gardener_id=? AND NOT EXISTS(SELECT 1 FROM collection WHERE id=current_collection_id AND gardener_id=?)");
    repair.bind(1, gardener); repair.bind(2, gardener); repair.bind(3, gardener);
    if (repair.step() != SQLITE_DONE) {
        return fail(sqlite3_errmsg(db));
    }

    if (a.kind == K::DeleteCollection) {
        Query last(db, "SELECT 1 FROM collection WHERE id=?1 AND gardener_id=?2 AND (SELECT count(*) FROM collection WHERE gardener_id=?2)=1");
        last.bind(1, a.id);
        last.bind(2, gardener);
        const int rc = last.step();
        if (rc == SQLITE_ROW) {
            return fail("A gardener must retain at least one collection", mv::MainverteError::Code::LastCollection);
        }

        if (rc != SQLITE_DONE) return fail(sqlite3_errmsg(db));
    }

    const char* sql = nullptr;
    switch (a.kind) {
    case K::Browse:
    case K::GetSpecimen:
    case K::ListSpecimens: break;
    case K::CreateCollection: sql = "INSERT INTO collection(name,gardener_id,created_at,modified_at) VALUES(?1,?2,unixepoch(),unixepoch())"; break;
    case K::RenameCollection: sql = "UPDATE collection SET name=?1,modified_at=unixepoch() WHERE gardener_id=?2 AND id=?3"; break;
    case K::SelectCollection: sql = "UPDATE settings SET current_collection_id=?3 WHERE id=1 AND current_gardener_id=?2 AND EXISTS(SELECT 1 FROM collection WHERE id=?3 AND gardener_id=?2)"; break;
    case K::DeleteCollection: sql = "DELETE FROM collection WHERE gardener_id=?2 AND id=?3 AND NOT EXISTS(SELECT 1 FROM specimen WHERE collection_id=?3)"; break;
    case K::CreateSpecimen: sql = "INSERT INTO specimen(display_name,collection_id,botanical_name,cultivar,created_at,modified_at) SELECT ?1,id,?4,?5,unixepoch(),unixepoch() FROM collection WHERE gardener_id=?2 AND id=?3"; break;
    case K::UpdateSpecimen: sql = "UPDATE specimen SET display_name=?1,botanical_name=?4,cultivar=?5,modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2)"; break;
    case K::MoveSpecimen: sql = "UPDATE specimen SET collection_id=?6,modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2) AND EXISTS(SELECT 1 FROM collection WHERE id=?6 AND gardener_id=?2)"; break;
    case K::ArchiveSpecimen: sql = "UPDATE specimen SET archived_at=unixepoch(),modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2)"; break;
    case K::RestoreSpecimen: sql = "UPDATE specimen SET archived_at=NULL,modified_at=unixepoch() WHERE id=?3 AND collection_id IN(SELECT id FROM collection WHERE gardener_id=?2)"; break;
    }

    if (sql) {
        if ((a.kind == K::CreateCollection || a.kind == K::RenameCollection || a.kind == K::CreateSpecimen || a.kind == K::UpdateSpecimen) && !valid(a.name)) {
            return fail("A name is required", mv::MainverteError::Code::InvalidInput);
        }

        Query write(db, sql);
        // Numbered parameters may leave unused positions; SQLite accepts binds to those slots.
        const int n = sqlite3_bind_parameter_count(write.stmt.get());
        if (n >= 1) write.bind(1, a.name);
        if (n >= 2) write.bind(2, gardener);
        if (n >= 3) write.bind(3, a.id);
        if (n >= 4) write.bind(4, a.botanical_name);
        if (n >= 5) write.bind(5, a.cultivar);
        if (n >= 6) write.bind(6, a.destination);
        if (write.step() != SQLITE_DONE) {
            return fail(sqlite3_errmsg(db));
        }

        if (sqlite3_changes(db) != 1) {
            if (a.kind == K::DeleteCollection) {
                Query occupied(db, "SELECT 1 FROM collection c JOIN specimen s ON s.collection_id=c.id WHERE c.id=? AND c.gardener_id=? LIMIT 1");
                occupied.bind(1, a.id);
                occupied.bind(2, gardener);
                if (occupied.step() == SQLITE_ROW) {
                    return fail("Collection contains active or archived plants", mv::MainverteError::Code::CollectionNotEmpty);
                }
            }

            return fail("Object unavailable or invalid ownership", mv::MainverteError::Code::NotFound);
        }
    }

    mv::Plants result;
    Query collections(db, "SELECT id,name,created_at,modified_at FROM collection WHERE gardener_id=? ORDER BY id");
    collections.bind(1, gardener);
    int rc;
    while ((rc = collections.step()) == SQLITE_ROW) {
        result.collections.push_back({collections.id(0), collections.text(1), collections.id(2), collections.id(3)});
    }

    if (rc != SQLITE_DONE) return fail(sqlite3_errmsg(db));
    Query reselection(db, "UPDATE settings SET current_collection_id=(SELECT id FROM collection WHERE gardener_id=? ORDER BY id LIMIT 1) WHERE id=1 AND current_gardener_id=? AND current_collection_id IS NULL");
    reselection.bind(1, gardener);
    reselection.bind(2, gardener);
    if (reselection.step() != SQLITE_DONE) return fail(sqlite3_errmsg(db));
    Query selection(db, "SELECT current_collection_id FROM settings WHERE id=1 AND current_gardener_id=?");
    selection.bind(1, gardener);
    if (selection.step() != SQLITE_ROW) return fail(sqlite3_errmsg(db));
    result.selected = selection.id(0);
    if (a.kind == K::ListSpecimens) {
        Query accessible(db, "SELECT id FROM collection WHERE id=? AND gardener_id=?");
        accessible.bind(1, a.id);
        accessible.bind(2, gardener);
        if (accessible.step() != SQLITE_ROW) return fail("Collection unavailable", mv::MainverteError::Code::NotFound);
    }

    Query plants(db, "SELECT id,collection_id,display_name,botanical_name,cultivar,archived_at IS NOT NULL,created_at,modified_at,archived_at FROM specimen WHERE collection_id IN(SELECT id FROM collection WHERE gardener_id=?3) AND ((?2=0 AND collection_id=?1) OR (?2>0 AND id=?2)) ORDER BY id DESC");
    plants.bind(1, a.kind == K::ListSpecimens ? a.id : result.selected);
    plants.bind(2, a.kind == K::GetSpecimen ? a.id : 0);
    plants.bind(3, gardener);
    while ((rc = plants.step()) == SQLITE_ROW) {
        result.specimens.push_back({plants.id(0), plants.id(1), plants.text(2), plants.text(3), plants.text(4), plants.id(5) != 0, plants.id(6), plants.id(7),
            sqlite3_column_type(plants.stmt.get(), 8) == SQLITE_NULL ? std::nullopt : std::optional<int64_t>(plants.id(8))});
    }

    if (rc != SQLITE_DONE) return fail(sqlite3_errmsg(db));
    if (a.kind == K::GetSpecimen && (a.id <= 0 || result.specimens.empty())) {
        return fail("Specimen unavailable", mv::MainverteError::Code::NotFound);
    }

    if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) return fail(sqlite3_errmsg(db));
    return result;
}
}  // namespace

bool mv::manage_plants(PlantAction action, std::string initial, MoveOnlyFunction<void(PlantsResult)> callback) {
    assert(callback);
    auto done = std::make_shared<MoveOnlyFunction<void(PlantsResult)>>(std::move(callback));
    const auto gardener = settings().current_gardener_id;
    const bool accepted = service::database().submit([action = std::move(action), initial = std::move(initial), gardener, done](sqlite3* db) {
        auto result = run(db, gardener, action, initial);
        platform::dispatch_callback([result = std::move(result), done, gardener]() mutable {
            if (result) {
                service::update_collection_selection(gardener, result->selected);
            }

            (*done)(std::move(result));
        });
    });
    if (!accepted) {
        platform::dispatch_callback([done] { (*done)(std::unexpected(MainverteError{MainverteError::Code::NotRunning, "Database is not running"})); });
    }

    return accepted;
}
