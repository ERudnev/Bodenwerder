#include "storage.h"

#include "placeholder.q1.h"
#include "retrospection.h"

#include <sqlite3.h>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fQSM/utility/bad_value.h>

namespace placeholder {

    using namespace fqsm::api;

    namespace {
        constexpr std::string_view sequence_owner_column = "owner";
        constexpr std::string_view sequence_ordinal_column = "ordinal";
        constexpr std::string_view sequence_value_column = "value";

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

        auto table_exists(sqlite3* db, std::string_view table) -> bool {
            sqlite3_stmt* statement = nullptr;
            if (sqlite3_prepare_v2(
                    db,
                    "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?",
                    -1,
                    &statement,
                    nullptr
                ) != SQLITE_OK)
                fail(db, "prepare sqlite_master");
            sqlite3_bind_text(statement, 1, table.data(), static_cast<int>(table.size()), SQLITE_TRANSIENT);
            const auto state = sqlite3_step(statement);
            sqlite3_finalize(statement);
            return state == SQLITE_ROW;
        }

        void stepDone(sqlite3* db, sqlite3_stmt* statement, std::string_view what) {
            if (sqlite3_step(statement) != SQLITE_DONE) {
                sqlite3_finalize(statement);
                fail(db, what);
            }
            sqlite3_finalize(statement);
        }

        auto sqlIdentifier(std::string_view name) -> std::string {
            return std::string{"\""} + std::string{name} + "\"";
        }

        auto sql_type(Retrospection::StorageAtom atom) -> std::string_view {
            switch (atom) {
                case Retrospection::StorageAtom::string: return "TEXT";
                case Retrospection::StorageAtom::integer: return "INTEGER";
                case Retrospection::StorageAtom::reference: return "INTEGER";
            }
            throw std::runtime_error("unsupported StorageAtom");
        }

        auto placeholders(std::size_t count) -> std::string {
            std::ostringstream out;
            for (std::size_t index = 0; index < count; ++index) {
                if (index != 0) out << ", ";
                out << '?';
            }
            return out.str();
        }

        auto build_quanta_insert_sql(const Retrospection& retrospection) -> std::string {
            std::ostringstream out;
            out << "INSERT INTO " << sqlIdentifier(retrospection.quantaTable()) << " (\"id\"";
            std::size_t persisted = 0;
            for (const auto& field : retrospection.quanta.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                out << ", " << sqlIdentifier(field.fieldPath);
                ++persisted;
            }
            out << ") VALUES (" << placeholders(1 + persisted) << ")";
            return out.str();
        }

        auto build_collection_insert_sql(const Retrospection& retrospection, const Retrospection::Field& field) -> std::string {
            std::ostringstream out;
            out << "INSERT INTO " << sqlIdentifier(retrospection.collectionTable(field))
                << " (" << sqlIdentifier(sequence_owner_column)
                << ", " << sqlIdentifier(sequence_ordinal_column)
                << ", " << sqlIdentifier(sequence_value_column)
                << ") VALUES (?, ?, ?)";
            return out.str();
        }

        auto build_globals_insert_sql(const Retrospection& retrospection, const Retrospection::Globals& globals) -> std::string {
            std::ostringstream out;
            out << "INSERT INTO " << sqlIdentifier(retrospection.globalsTable()) << " (\"key\"";
            for (const auto& field : globals.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                out << ", " << sqlIdentifier(field.fieldPath);
            }
            out << ") VALUES (0";
            for (const auto& field : globals.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                out << ", ?";
            }
            out << ")";
            return out.str();
        }

        auto build_quanta_select_sql(const Retrospection& retrospection) -> std::string {
            std::ostringstream out;
            out << "SELECT " << sqlIdentifier("id");
            for (const auto& field : retrospection.quanta.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                out << ", " << sqlIdentifier(field.fieldPath);
            }
            out << " FROM " << sqlIdentifier(retrospection.quantaTable())
                << " ORDER BY " << sqlIdentifier("id");
            return out.str();
        }

        auto build_collection_select_sql(const Retrospection& retrospection, const Retrospection::Field& field) -> std::string {
            std::ostringstream out;
            out << "SELECT "
                << sqlIdentifier(sequence_owner_column) << ", "
                << sqlIdentifier(sequence_ordinal_column) << ", "
                << sqlIdentifier(sequence_value_column)
                << " FROM " << sqlIdentifier(retrospection.collectionTable(field))
                << " ORDER BY " << sqlIdentifier(sequence_owner_column) << ", " << sqlIdentifier(sequence_ordinal_column);
            return out.str();
        }

        auto build_globals_select_sql(const Retrospection& retrospection, const Retrospection::Globals& globals) -> std::string {
            std::ostringstream out;
            out << "SELECT ";
            bool first = true;
            for (const auto& field : globals.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                if (!first) out << ", ";
                out << sqlIdentifier(field.fieldPath);
                first = false;
            }
            out << " FROM " << sqlIdentifier(retrospection.globalsTable())
                << " WHERE " << sqlIdentifier("key") << " = 0";
            return out.str();
        }

        void create_quanta_table(sqlite3* db, const Retrospection& retrospection) {
            std::ostringstream out;
            out << "CREATE TABLE " << sqlIdentifier(retrospection.quantaTable()) << " (\n"
                << "    id INTEGER PRIMARY KEY NOT NULL";
            for (const auto& field : retrospection.quanta.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                out << ",\n    " << sqlIdentifier(field.fieldPath) << ' ' << sql_type(field.atom) << " NOT NULL";
            }
            out << "\n)";
            exec(db, out.str().c_str());
        }

        void create_collection_table(sqlite3* db, const Retrospection& retrospection, const Retrospection::Field& field) {
            std::ostringstream out;
            out << "CREATE TABLE " << sqlIdentifier(retrospection.collectionTable(field)) << " (\n"
                << "    " << sqlIdentifier(sequence_owner_column) << " INTEGER NOT NULL,\n"
                << "    " << sqlIdentifier(sequence_ordinal_column) << " INTEGER NOT NULL,\n"
                << "    " << sqlIdentifier(sequence_value_column) << ' ' << sql_type(field.atom) << " NOT NULL,\n"
                << "    PRIMARY KEY (" << sqlIdentifier(sequence_owner_column) << ", " << sqlIdentifier(sequence_ordinal_column) << ")\n"
                << ")";
            exec(db, out.str().c_str());
        }

        void create_globals_table(sqlite3* db, const Retrospection& retrospection, const Retrospection::Globals& globals) {
            std::ostringstream out;
            out << "CREATE TABLE " << sqlIdentifier(retrospection.globalsTable()) << " (\n"
                << "    key INTEGER PRIMARY KEY NOT NULL CHECK (key = 0)";
            for (const auto& field : globals.fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                out << ",\n    " << sqlIdentifier(field.fieldPath) << ' ' << sql_type(field.atom) << " NOT NULL";
            }
            out << "\n)";
            exec(db, out.str().c_str());
        }

        template<typename Meta>
        auto has_storage_schema(sqlite3* db) -> bool {
            const auto retrospection = Meta::retrospection();
            if (!table_exists(db, retrospection.quantaTable())) return false;
            for (const auto& field : retrospection.collections) {
                if (!table_exists(db, retrospection.collectionTable(field))) return false;
            }
            if (retrospection.globals.has_value() && !table_exists(db, retrospection.globalsTable())) return false;
            return true;
        }

        template<typename Meta>
        void rewrite_tables(sqlite3* db) {
            const auto retrospection = Meta::retrospection();

            for (const auto& field : retrospection.collections)
                exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(retrospection.collectionTable(field))).c_str());
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(retrospection.quantaTable())).c_str());
            if (retrospection.globals.has_value())
                exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(retrospection.globalsTable())).c_str());

            create_quanta_table(db, retrospection);
            for (const auto& field : retrospection.collections)
                create_collection_table(db, retrospection, field);
            if (retrospection.globals.has_value())
                create_globals_table(db, retrospection, retrospection.globals.value());
        }

        template<typename Meta>
        void save_quanta(sqlite3* db, Reading context) {
            const auto retrospection = Meta::retrospection();
            sqlite3_stmt* statement = nullptr;
            const auto sql = build_quanta_insert_sql(retrospection);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", retrospection.quantaTable()));

            for (const auto entry : context->aspect<Meta>().items()) {
                sqlite3_reset(statement);
                sqlite3_clear_bindings(statement);
                sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(entry.id.raw()));

                int bindIndex = 2;
                for (const auto& field : retrospection.quanta.fields) {
                    if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                    if (!field.bindLeaf) {
                        sqlite3_finalize(statement);
                        throw std::runtime_error(std::format("{}:{} has no leaf codec", retrospection.aspectName, field.fieldPath));
                    }
                    field.bindLeaf(
                        statement,
                        bindIndex++,
                        detail::retrospection::descend(std::addressof(entry.value), field.path)
                    );
                }

                if (sqlite3_step(statement) != SQLITE_DONE) {
                    sqlite3_finalize(statement);
                    fail(db, std::format("insert {}", retrospection.quantaTable()));
                }
            }

            sqlite3_finalize(statement);
        }

        template<typename Meta>
        void save_collections(sqlite3* db, Reading context) {
            const auto retrospection = Meta::retrospection();

            for (const auto& field : retrospection.collections) {
                sqlite3_stmt* statement = nullptr;
                const auto sql = build_collection_insert_sql(retrospection, field);
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                    fail(db, std::format("prepare {}", retrospection.collectionTable(field)));

                for (const auto entry : context->aspect<Meta>().items()) {
                    if (!field.countElements || !field.elementAt || !field.bindLeaf) {
                        sqlite3_finalize(statement);
                        throw std::runtime_error(std::format("{}:{} has incomplete collection codec", retrospection.aspectName, field.fieldPath));
                    }

                    const auto* container = detail::retrospection::descend(std::addressof(entry.value), field.path);
                    const auto count = field.countElements(container);
                    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
                        sqlite3_reset(statement);
                        sqlite3_clear_bindings(statement);
                        sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(entry.id.raw()));
                        sqlite3_bind_int(statement, 2, static_cast<int>(ordinal));
                        field.bindLeaf(statement, 3, field.elementAt(container, ordinal));

                        if (sqlite3_step(statement) != SQLITE_DONE) {
                            sqlite3_finalize(statement);
                            fail(db, std::format("insert {}", retrospection.collectionTable(field)));
                        }
                    }
                }

                sqlite3_finalize(statement);
            }
        }

        template<typename Meta>
        void save_globals(sqlite3* db, Reading context) {
            const auto retrospection = Meta::retrospection();
            if (!retrospection.globals.has_value()) return;

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_globals_insert_sql(retrospection, retrospection.globals.value());
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", retrospection.globalsTable()));

            const auto& global = with<Meta>::get_global(context);
            int bindIndex = 1;
            for (const auto& field : retrospection.globals->fields) {
                if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                if (!field.bindLeaf) {
                    sqlite3_finalize(statement);
                    throw std::runtime_error(std::format("{}:{} has no global leaf codec", retrospection.aspectName, field.fieldPath));
                }
                field.bindLeaf(
                    statement,
                    bindIndex++,
                    detail::retrospection::descend(std::addressof(global), field.path)
                );
            }

            stepDone(db, statement, std::format("insert {}", retrospection.globalsTable()));
        }

        template<typename Meta>
        void save_aspect(sqlite3* db, Reading context) {
            save_quanta<Meta>(db, context);
            save_collections<Meta>(db, context);
            save_globals<Meta>(db, context);
        }

        template<typename Meta>
        void load_quanta(sqlite3* db, Writing context) {
            const auto retrospection = Meta::retrospection();
            sqlite3_stmt* statement = nullptr;
            const auto sql = build_quanta_select_sql(retrospection);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", retrospection.quantaTable()));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                const auto id = typename Meta::Id{static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))};
                with<Meta>::restore(context, id, fqsm::utility::BadValue{});
                auto quantum = with<Meta>::modify(context, id);

                int columnIndex = 1;
                for (const auto& field : retrospection.quanta.fields) {
                    if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                    if (!field.readLeaf) {
                        sqlite3_finalize(statement);
                        throw std::runtime_error(std::format("{}:{} has no leaf reader", retrospection.aspectName, field.fieldPath));
                    }
                    field.readLeaf(statement, columnIndex++, detail::retrospection::descend(std::addressof(*quantum), field.path));
                }
            }

            sqlite3_finalize(statement);
        }

        template<typename Meta>
        void load_collections(sqlite3* db, Writing context) {
            const auto retrospection = Meta::retrospection();

            for (const auto& field : retrospection.collections) {
                sqlite3_stmt* statement = nullptr;
                const auto sql = build_collection_select_sql(retrospection, field);
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                    fail(db, std::format("prepare {}", retrospection.collectionTable(field)));

                while (sqlite3_step(statement) == SQLITE_ROW) {
                    const auto owner = typename Meta::Id{static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))};
                    auto quantum = with<Meta>::modify(context, owner);
                    if (!field.appendElement) {
                        sqlite3_finalize(statement);
                        throw std::runtime_error(std::format("{}:{} has no collection loader", retrospection.aspectName, field.fieldPath));
                    }
                    field.appendElement(statement, 2, detail::retrospection::descend(std::addressof(*quantum), field.path));
                }

                sqlite3_finalize(statement);
            }
        }

        template<typename Meta>
        void load_globals(sqlite3* db, Writing context) {
            const auto retrospection = Meta::retrospection();
            if (!retrospection.globals.has_value()) return;

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_globals_select_sql(retrospection, retrospection.globals.value());
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", retrospection.globalsTable()));

            if (sqlite3_step(statement) == SQLITE_ROW) {
                auto global = with<Meta>::modify_global(context);
                int columnIndex = 0;
                for (const auto& field : retrospection.globals->fields) {
                    if (field.persistence != Retrospection::Persistence::scalar_column) continue;
                    if (!field.readLeaf) {
                        sqlite3_finalize(statement);
                        throw std::runtime_error(std::format("{}:{} has no global leaf reader", retrospection.aspectName, field.fieldPath));
                    }
                    field.readLeaf(statement, columnIndex++, detail::retrospection::descend(std::addressof(*global), field.path));
                }
            }

            sqlite3_finalize(statement);
        }

        template<typename Meta>
        void load_aspect(sqlite3* db, Writing context) {
            load_quanta<Meta>(db, context);
            load_collections<Meta>(db, context);
            load_globals<Meta>(db, context);
        }

    }

    bool loadRegistry(Writing context, std::filesystem::path dbPath) {
        if (!std::filesystem::exists(dbPath)) return false;

        sqlite3* db = nullptr;
        if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }

        if (!has_storage_schema<Person>(db) || !has_storage_schema<Family>(db)) {
            sqlite3_close(db);
            return false;
        }

        load_aspect<Person>(db, context);
        load_aspect<Family>(db, context);

        sqlite3_close(db);
        return true;
    }

    void saveRegistry(Reading context, std::filesystem::path dbPath) {
        std::filesystem::create_directories(dbPath.parent_path());

        sqlite3* db = nullptr;
        if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK)
            fail(db, "open");

        try {
            exec(db, "BEGIN");

            rewrite_tables<Person>(db);
            rewrite_tables<Family>(db);

            save_aspect<Person>(db, context);
            save_aspect<Family>(db, context);

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
