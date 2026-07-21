#pragma once

#include <fQSM/api/interface.h>
#include <fQSM/aspect/persistency.h>
#include <fQSM/meta/alias.h>
#include <fQSM/utility/bad_value.h>
#include <pQRF/database/retrospection.h>
#include <pQRF/database/sql.h>
#include <pQRF/database/storage.h>

#include <sqlite3.h>

#include <cstdint>
#include <format>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fqsm::processing::persistency::database::detail {

    using namespace fqsm::api;
    using fqsm::aspect::Collection;
    using fqsm::aspect::Field;

    constexpr std::string_view sequence_owner_column = "owner";
    constexpr std::string_view sequence_ordinal_column = "ordinal";
    constexpr std::string_view sequence_value_column = "value";

    [[noreturn]] inline void fail(sqlite3* db, std::string_view what) {
        throw std::runtime_error(std::format("{}: {}", what, db ? sqlite3_errmsg(db) : "no db"));
    }

    inline void exec(sqlite3* db, const char* sql) {
        char* error = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
            const std::string message = error ? error : "sqlite3_exec failed";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

    inline auto table_exists(sqlite3* db, std::string_view table) -> bool {
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

    inline void stepDone(sqlite3* db, sqlite3_stmt* statement, std::string_view what) {
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            fail(db, what);
        }
        sqlite3_finalize(statement);
    }

    inline auto sqlIdentifier(std::string_view name) -> std::string {
        return std::string{"\""} + std::string{name} + "\"";
    }

    inline auto qualify(std::string_view lhs, std::string_view rhs) -> std::string {
        return std::string{lhs} + "." + std::string{rhs};
    }

    inline auto placeholders(std::size_t count) -> std::string {
        std::ostringstream out;
        for (std::size_t index = 0; index < count; ++index) {
            if (index != 0) out << ", ";
            out << '?';
        }
        return out.str();
    }

    struct LayoutColumn {
        std::string_view name;
        std::string_view sqlType;
    };

    template<typename Meta>
    struct LayoutDesc {
        std::string_view aspectName{};
        std::vector<LayoutColumn> one_fields{};
        std::vector<LayoutColumn> one_collections{};
        std::vector<LayoutColumn> all_fields{};
        std::vector<LayoutColumn> all_collections{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            sql::atom<Leaf>::require();
            one_fields.push_back({slot.name, sql::atom<Leaf>::sql_name});
        }

        template<auto... Members>
        void one(Collection<Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            using Elem = typename Container::value_type;
            sql::atom<Elem>::require();
            one_collections.push_back({slot.name, sql::atom<Elem>::sql_name});
        }

        template<auto... Members>
        void all(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(std::declval<fqsm::GlobalValue<Meta>&>()))>;
            sql::atom<Leaf>::require();
            all_fields.push_back({slot.name, sql::atom<Leaf>::sql_name});
        }

        template<auto... Members>
        void all(Collection<Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<fqsm::GlobalValue<Meta>&>()))>;
            using Elem = typename Container::value_type;
            sql::atom<Elem>::require();
            all_collections.push_back({slot.name, sql::atom<Elem>::sql_name});
        }
    };

    template<typename Meta>
    auto layout_of() -> LayoutDesc<Meta> {
        LayoutDesc<Meta> layout{};
        Meta::describe(layout);
        return layout;
    }

    inline auto quanta_table(std::string_view aspectName) -> std::string {
        return std::string{aspectName};
    }

    inline auto globals_table(std::string_view aspectName) -> std::string {
        return qualify(aspectName, "all");
    }

    inline auto one_collection_table(std::string_view aspectName, std::string_view field) -> std::string {
        return qualify(aspectName, field);
    }

    inline auto all_collection_table(std::string_view aspectName, std::string_view field) -> std::string {
        return qualify(globals_table(aspectName), field);
    }

    template<typename Meta>
    auto build_quanta_insert_sql(const LayoutDesc<Meta>& layout) -> std::string {
        std::ostringstream out;
        out << "INSERT INTO " << sqlIdentifier(quanta_table(layout.aspectName)) << " (\"id\"";
        for (const auto& field : layout.one_fields)
            out << ", " << sqlIdentifier(field.name);
        out << ") VALUES (" << placeholders(1 + layout.one_fields.size()) << ")";
        return out.str();
    }

    inline auto build_collection_insert_sql(std::string_view table) -> std::string {
        std::ostringstream out;
        out << "INSERT INTO " << sqlIdentifier(table)
            << " (" << sqlIdentifier(sequence_owner_column)
            << ", " << sqlIdentifier(sequence_ordinal_column)
            << ", " << sqlIdentifier(sequence_value_column)
            << ") VALUES (?, ?, ?)";
        return out.str();
    }

    template<typename Meta>
    auto build_globals_insert_sql(const LayoutDesc<Meta>& layout) -> std::string {
        std::ostringstream out;
        out << "INSERT INTO " << sqlIdentifier(globals_table(layout.aspectName)) << " (\"key\"";
        for (const auto& field : layout.all_fields)
            out << ", " << sqlIdentifier(field.name);
        out << ") VALUES (0";
        for (std::size_t index = 0; index < layout.all_fields.size(); ++index)
            out << ", ?";
        out << ")";
        return out.str();
    }

    template<typename Meta>
    auto build_quanta_select_sql(const LayoutDesc<Meta>& layout) -> std::string {
        std::ostringstream out;
        out << "SELECT " << sqlIdentifier("id");
        for (const auto& field : layout.one_fields)
            out << ", " << sqlIdentifier(field.name);
        out << " FROM " << sqlIdentifier(quanta_table(layout.aspectName))
            << " ORDER BY " << sqlIdentifier("id");
        return out.str();
    }

    inline auto build_collection_select_sql(std::string_view table) -> std::string {
        std::ostringstream out;
        out << "SELECT "
            << sqlIdentifier(sequence_owner_column) << ", "
            << sqlIdentifier(sequence_ordinal_column) << ", "
            << sqlIdentifier(sequence_value_column)
            << " FROM " << sqlIdentifier(table)
            << " ORDER BY " << sqlIdentifier(sequence_owner_column) << ", "
            << sqlIdentifier(sequence_ordinal_column);
        return out.str();
    }

    template<typename Meta>
    auto build_globals_select_sql(const LayoutDesc<Meta>& layout) -> std::string {
        std::ostringstream out;
        out << "SELECT ";
        for (std::size_t index = 0; index < layout.all_fields.size(); ++index) {
            if (index != 0) out << ", ";
            out << sqlIdentifier(layout.all_fields[index].name);
        }
        out << " FROM " << sqlIdentifier(globals_table(layout.aspectName))
            << " WHERE " << sqlIdentifier("key") << " = 0";
        return out.str();
    }

    template<typename Meta>
    void create_quanta_table(sqlite3* db, const LayoutDesc<Meta>& layout) {
        std::ostringstream out;
        out << "CREATE TABLE " << sqlIdentifier(quanta_table(layout.aspectName)) << " (\n"
            << "    id INTEGER PRIMARY KEY NOT NULL";
        for (const auto& field : layout.one_fields)
            out << ",\n    " << sqlIdentifier(field.name) << ' ' << field.sqlType << " NOT NULL";
        out << "\n)";
        exec(db, out.str().c_str());
    }

    inline void create_collection_table(sqlite3* db, std::string_view table, std::string_view sqlType) {
        std::ostringstream out;
        out << "CREATE TABLE " << sqlIdentifier(table) << " (\n"
            << "    " << sqlIdentifier(sequence_owner_column) << " INTEGER NOT NULL,\n"
            << "    " << sqlIdentifier(sequence_ordinal_column) << " INTEGER NOT NULL,\n"
            << "    " << sqlIdentifier(sequence_value_column) << ' ' << sqlType << " NOT NULL,\n"
            << "    PRIMARY KEY (" << sqlIdentifier(sequence_owner_column) << ", "
            << sqlIdentifier(sequence_ordinal_column) << ")\n"
            << ")";
        exec(db, out.str().c_str());
    }

    template<typename Meta>
    void create_globals_table(sqlite3* db, const LayoutDesc<Meta>& layout) {
        std::ostringstream out;
        out << "CREATE TABLE " << sqlIdentifier(globals_table(layout.aspectName)) << " (\n"
            << "    key INTEGER PRIMARY KEY NOT NULL CHECK (key = 0)";
        for (const auto& field : layout.all_fields)
            out << ",\n    " << sqlIdentifier(field.name) << ' ' << field.sqlType << " NOT NULL";
        out << "\n)";
        exec(db, out.str().c_str());
    }

    template<typename Meta>
    auto has_storage_schema(sqlite3* db) -> bool {
        const auto layout = layout_of<Meta>();
        if (!table_exists(db, quanta_table(layout.aspectName))) return false;
        for (const auto& field : layout.one_collections) {
            if (!table_exists(db, one_collection_table(layout.aspectName, field.name))) return false;
        }
        for (const auto& field : layout.all_collections) {
            if (!table_exists(db, all_collection_table(layout.aspectName, field.name))) return false;
        }
        if (!layout.all_fields.empty() && !table_exists(db, globals_table(layout.aspectName))) return false;
        return true;
    }

    template<typename Meta>
    void rewrite_tables(sqlite3* db) {
        const auto layout = layout_of<Meta>();

        for (const auto& field : layout.one_collections)
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(one_collection_table(layout.aspectName, field.name))).c_str());
        for (const auto& field : layout.all_collections)
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(all_collection_table(layout.aspectName, field.name))).c_str());
        exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(quanta_table(layout.aspectName))).c_str());
        if (!layout.all_fields.empty())
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(globals_table(layout.aspectName))).c_str());

        create_quanta_table(db, layout);
        for (const auto& field : layout.one_collections)
            create_collection_table(db, one_collection_table(layout.aspectName, field.name), field.sqlType);
        for (const auto& field : layout.all_collections)
            create_collection_table(db, all_collection_table(layout.aspectName, field.name), field.sqlType);
        if (!layout.all_fields.empty())
            create_globals_table(db, layout);
    }

    template<typename Meta>
    struct BindOneFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        const typename Meta::Quantum& quantum;
        int bindIndex = 2;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            sql::bind(statement, bindIndex++, slot.get(quantum));
        }

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct ReadOneFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        typename Meta::Quantum& quantum;
        int columnIndex = 1;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            sql::read(statement, columnIndex++, slot.get(quantum));
        }

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct BindAllFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        const fqsm::GlobalValue<Meta>& global;
        int bindIndex = 1;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...> slot) {
            sql::bind(statement, bindIndex++, slot.get(global));
        }

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct ReadAllFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        fqsm::GlobalValue<Meta>& global;
        int columnIndex = 0;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...> slot) {
            sql::read(statement, columnIndex++, slot.get(global));
        }

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct SaveOneCollectionsDesc {
        sqlite3* db = nullptr;
        Reading context;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...> slot) {
            const auto table = one_collection_table(aspectName, slot.name);
            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_insert_sql(table);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            for (const auto entry : context->aspect<Meta>().items()) {
                const auto& container = slot.get(entry.value);
                for (std::size_t ordinal = 0; ordinal < container.size(); ++ordinal) {
                    sqlite3_reset(statement);
                    sqlite3_clear_bindings(statement);
                    sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(entry.id.raw()));
                    sqlite3_bind_int(statement, 2, static_cast<int>(ordinal));
                    sql::bind(statement, 3, container[ordinal]);

                    if (sqlite3_step(statement) != SQLITE_DONE) {
                        sqlite3_finalize(statement);
                        fail(db, std::format("insert {}", table));
                    }
                }
            }

            sqlite3_finalize(statement);
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct SaveAllCollectionsDesc {
        sqlite3* db = nullptr;
        const fqsm::GlobalValue<Meta>& global;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...> slot) {
            const auto table = all_collection_table(aspectName, slot.name);
            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_insert_sql(table);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            const auto& container = slot.get(global);
            for (std::size_t ordinal = 0; ordinal < container.size(); ++ordinal) {
                sqlite3_reset(statement);
                sqlite3_clear_bindings(statement);
                sqlite3_bind_int64(statement, 1, 0);
                sqlite3_bind_int(statement, 2, static_cast<int>(ordinal));
                sql::bind(statement, 3, container[ordinal]);

                if (sqlite3_step(statement) != SQLITE_DONE) {
                    sqlite3_finalize(statement);
                    fail(db, std::format("insert {}", table));
                }
            }

            sqlite3_finalize(statement);
        }
    };

    template<typename Meta>
    struct LoadOneCollectionsDesc {
        sqlite3* db = nullptr;
        Writing context;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            using Elem = typename Container::value_type;

            const auto table = one_collection_table(aspectName, slot.name);
            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                const auto owner = typename Meta::Id{
                    static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
                };
                auto quantum = with<Meta>::modify(context, owner);
                Elem value = fqsm::utility::BadValue{};
                sql::read(statement, 2, value);
                slot.get(*quantum).push_back(std::move(value));
            }

            sqlite3_finalize(statement);
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct LoadAllCollectionsDesc {
        sqlite3* db = nullptr;
        fqsm::GlobalValue<Meta>& global;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(global))>;
            using Elem = typename Container::value_type;

            const auto table = all_collection_table(aspectName, slot.name);
            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                Elem value = fqsm::utility::BadValue{};
                sql::read(statement, 2, value);
                slot.get(global).push_back(std::move(value));
            }

            sqlite3_finalize(statement);
        }
    };

    template<typename Meta>
    void save_quanta(sqlite3* db, Reading context) {
        const auto layout = layout_of<Meta>();
        sqlite3_stmt* statement = nullptr;
        const auto sql = build_quanta_insert_sql(layout);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", quanta_table(layout.aspectName)));

        for (const auto entry : context->aspect<Meta>().items()) {
            sqlite3_reset(statement);
            sqlite3_clear_bindings(statement);
            sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(entry.id.raw()));

            BindOneFieldsDesc<Meta> binder{statement, entry.value, 2};
            Meta::describe(binder);

            if (sqlite3_step(statement) != SQLITE_DONE) {
                sqlite3_finalize(statement);
                fail(db, std::format("insert {}", quanta_table(layout.aspectName)));
            }
        }

        sqlite3_finalize(statement);
    }

    template<typename Meta>
    void save_collections(sqlite3* db, Reading context) {
        SaveOneCollectionsDesc<Meta> desc{db, context, {}};
        Meta::describe(desc);
    }

    template<typename Meta>
    void save_global_collections(sqlite3* db, Reading context) {
        const auto layout = layout_of<Meta>();
        if (layout.all_collections.empty()) return;

        const auto& global = with<Meta>::get_global(context);
        SaveAllCollectionsDesc<Meta> desc{db, global, {}};
        Meta::describe(desc);
    }

    template<typename Meta>
    void save_globals(sqlite3* db, Reading context) {
        const auto layout = layout_of<Meta>();
        if (layout.all_fields.empty()) return;

        sqlite3_stmt* statement = nullptr;
        const auto sql = build_globals_insert_sql(layout);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", globals_table(layout.aspectName)));

        const auto& global = with<Meta>::get_global(context);
        BindAllFieldsDesc<Meta> binder{statement, global, 1};
        Meta::describe(binder);

        stepDone(db, statement, std::format("insert {}", globals_table(layout.aspectName)));
    }

    template<typename Meta>
    void save_aspect(sqlite3* db, Reading context) {
        save_quanta<Meta>(db, context);
        save_collections<Meta>(db, context);
        save_globals<Meta>(db, context);
        save_global_collections<Meta>(db, context);
    }

    template<typename Meta>
    void load_quanta(sqlite3* db, Writing context) {
        const auto layout = layout_of<Meta>();
        sqlite3_stmt* statement = nullptr;
        const auto sql = build_quanta_select_sql(layout);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", quanta_table(layout.aspectName)));

        while (sqlite3_step(statement) == SQLITE_ROW) {
            const auto id = typename Meta::Id{
                static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
            };
            with<Meta>::restore(context, id, fqsm::utility::BadValue{});
            auto quantum = with<Meta>::modify(context, id);

            ReadOneFieldsDesc<Meta> reader{statement, *quantum, 1};
            Meta::describe(reader);
        }

        sqlite3_finalize(statement);
    }

    template<typename Meta>
    void load_collections(sqlite3* db, Writing context) {
        LoadOneCollectionsDesc<Meta> desc{db, context, {}};
        Meta::describe(desc);
    }

    template<typename Meta>
    void load_global_collections(sqlite3* db, Writing context) {
        const auto layout = layout_of<Meta>();
        if (layout.all_collections.empty()) return;

        auto global = with<Meta>::modify_global(context);
        LoadAllCollectionsDesc<Meta> desc{db, *global, {}};
        Meta::describe(desc);
    }

    template<typename Meta>
    void load_globals(sqlite3* db, Writing context) {
        const auto layout = layout_of<Meta>();
        if (layout.all_fields.empty()) return;

        sqlite3_stmt* statement = nullptr;
        const auto sql = build_globals_select_sql(layout);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", globals_table(layout.aspectName)));

        if (sqlite3_step(statement) == SQLITE_ROW) {
            auto global = with<Meta>::modify_global(context);
            ReadAllFieldsDesc<Meta> reader{statement, *global, 0};
            Meta::describe(reader);
        }

        sqlite3_finalize(statement);
    }

    template<typename Meta>
    void load_aspect(sqlite3* db, Writing context) {
        load_quanta<Meta>(db, context);
        load_collections<Meta>(db, context);
        load_globals<Meta>(db, context);
        load_global_collections<Meta>(db, context);
    }

    template<typename Meta>
    void clear_aspect(Writing context) {
        std::vector<typename Meta::Id> ids;
        for (const auto entry : context->aspect<Meta>().items())
            ids.push_back(entry.id);
        for (const auto id : ids)
            with<Meta>::remove(context, id);
    }

}

namespace fqsm::processing::persistency::database {

    using namespace fqsm::api;
    using namespace detail;

    template<typename Meta>
    struct ArchiveOpsFor final : ArchiveOps {
        bool present(DatabaseProxy& db) override {
            return has_storage_schema<Meta>(db.engine());
        }

        void clear(Writing context) override {
            clear_aspect<Meta>(context);
        }

        void pull(Writing context, DatabaseProxy& db) override {
            load_aspect<Meta>(db.engine(), context);
        }

        void push(Reading context, DatabaseProxy& db) override {
            rewrite_tables<Meta>(db.engine());
            save_aspect<Meta>(db.engine(), context);
        }
    };

    template<fqsm::meta::category::Any Meta>
        requires HasRetrospection<Meta>
    auto ArchiveOps::of() -> std::shared_ptr<ArchiveOps> {
        return std::make_shared<ArchiveOpsFor<Meta>>();
    }

    template<fqsm::meta::category::Any Meta>
        requires HasRetrospection<Meta>
    auto aspect() -> Schema {
        return persistency::aspect<Meta>(std::shared_ptr<AspectArchive>{ArchiveOps::of<Meta>()});
    }

}
