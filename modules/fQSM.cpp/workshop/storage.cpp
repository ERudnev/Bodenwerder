#include "storage.h"

#include "placeholder.q1.h"

#include <base/serialization.h>
#include <sqlite3.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace placeholder {

    using namespace fqsm::api;

    namespace {

        [[noreturn]] void fail(sqlite3* db, std::string_view what) {
            throw std::runtime_error(std::format("{}: {}", what, db ? sqlite3_errmsg(db) : "no db"));
        }

        void exec(sqlite3* db, const char* sql) {
            char* error = nullptr;
            if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
                const std::string message = error ? error : "sqlite3_exec failed";
                sqlite3_free(error);
                throw std::runtime_error(message);
            }
        }

        void insertRow(sqlite3* db, const char* sql, std::uint64_t id, const std::string& payload) {
            sqlite3_stmt* statement = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
                fail(db, "prepare insert");
            sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(id));
            sqlite3_bind_text(statement, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(statement) != SQLITE_DONE) {
                sqlite3_finalize(statement);
                fail(db, "step insert");
            }
            sqlite3_finalize(statement);
        }

    }

    void saveRegistry(Reading context, std::filesystem::path dbPath) {
        std::filesystem::create_directories(dbPath.parent_path());

        sqlite3* db = nullptr;
        if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK)
            fail(db, "open");

        try {
            exec(db, "BEGIN");
            exec(db, R"(DROP TABLE IF EXISTS "Person.table")");
            exec(db, R"(DROP TABLE IF EXISTS "Family.table")");
            exec(db, R"(DROP TABLE IF EXISTS "Family.globals")");
            exec(db, R"(
                CREATE TABLE "Person.table" (
                    id INTEGER PRIMARY KEY NOT NULL,
                    quantum TEXT NOT NULL
                )
            )");
            exec(db, R"(
                CREATE TABLE "Family.table" (
                    id INTEGER PRIMARY KEY NOT NULL,
                    quantum TEXT NOT NULL
                )
            )");
            exec(db, R"(
                CREATE TABLE "Family.globals" (
                    key INTEGER PRIMARY KEY NOT NULL CHECK (key = 0),
                    value TEXT NOT NULL
                )
            )");

            for (const auto entry : context->aspect<Person>().items()) {
                insertRow(
                    db,
                    R"(INSERT INTO "Person.table" (id, quantum) VALUES (?, ?))",
                    entry.id.raw(),
                    base::encoded(entry.value)
                );
            }

            for (const auto entry : context->aspect<Family>().items()) {
                insertRow(
                    db,
                    R"(INSERT INTO "Family.table" (id, quantum) VALUES (?, ?))",
                    entry.id.raw(),
                    base::encoded(entry.value)
                );
            }

            insertRow(
                db,
                R"(INSERT INTO "Family.globals" (key, value) VALUES (?, ?))",
                0,
                base::encoded(with<Family>::get_global(context))
            );

            exec(db, "COMMIT");
        } catch (...) {
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw;
        }

        sqlite3_close(db);
        base::message("saved registry to {}", dbPath.string());
    }

}
