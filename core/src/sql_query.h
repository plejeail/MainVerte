#pragma once

#include <string>

#include <sqlite3.h>

#include "database.h"
#include "mv_definitions.h"

namespace detail {

// Short-lived prepared query. Bind failures prevent stepping the statement.
struct Query {
    sqlite3* db;
    Database::statement_ptr stmt{nullptr, sqlite3_finalize};
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

}  //  namespace detail
