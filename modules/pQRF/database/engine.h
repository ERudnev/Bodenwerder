#pragma once

#include <fQSM/api/interface.h>
#include <fQSM/aspect/persistency.h>
#include <fQSM/meta/alias.h>
#include <fQSM/processing/orchestrators/realm.h>
#include <fQSM/utility/bad_value.h>
#include <pQRF/database/form_tree.h>
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

    inline auto placeholders(std::size_t count) -> std::string {
        std::ostringstream out;
        for (std::size_t index = 0; index < count; ++index) {
            if (index != 0) out << ", ";
            out << '?';
        }
        return out.str();
    }

    template<typename Meta>
    struct LayoutDesc {
        std::string_view aspectName{};
        std::vector<LayoutColumn> one_fields{};
        std::vector<LayoutCollection> one_collections{};
        std::vector<LayoutColumn> all_fields{};
        std::vector<LayoutCollection> all_collections{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            append_product_or_atom<Leaf>(slot.name, one_fields);
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            assert_collection_elem<Elem, Container>();
            register_collection_layout<Elem, Container>(
                std::string{slot.name}, slot.key_name, {}, one_collections);
        }

        template<auto... Members>
        void all(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(std::declval<fqsm::GlobalValue<Meta>&>()))>;
            append_product_or_atom<Leaf>(slot.name, all_fields);
        }

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<fqsm::GlobalValue<Meta>&>()))>;
            assert_collection_elem<Elem, Container>();
            register_collection_layout<Elem, Container>(
                std::string{slot.name}, slot.key_name, {}, all_collections);
        }
    };

    template<typename Meta>
    auto layout_of() -> LayoutDesc<Meta> {
        LayoutDesc<Meta> layout{};
        fqsm::aspect::Retrospection<Meta>::describe(layout);
        return layout;
    }

    inline auto quanta_table(std::string_view aspectName) -> std::string {
        return std::string{aspectName};
    }

    inline auto globals_table(std::string_view aspectName) -> std::string {
        return qualify(aspectName, "all");
    }

    inline auto one_collection_table(std::string_view aspectName, std::string_view relative) -> std::string {
        return qualify(aspectName, relative);
    }

    inline auto all_collection_table(std::string_view aspectName, std::string_view relative) -> std::string {
        return qualify(globals_table(aspectName), relative);
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

    inline auto build_collection_insert_sql(std::string_view table, const LayoutCollection& collection)
        -> std::string {
        const auto side = collection.all_value_side_columns();
        std::ostringstream out;
        out << "INSERT INTO " << sqlIdentifier(table)
            << " (" << sqlIdentifier(sequence_owner_column);
        for (const auto& column : side)
            out << ", " << sqlIdentifier(column.name);
        out << ") VALUES (" << placeholders(1 + side.size()) << ")";
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

    inline auto build_collection_select_sql(std::string_view table, const LayoutCollection& collection)
        -> std::string {
        const auto side = collection.all_value_side_columns();
        std::ostringstream out;
        out << "SELECT " << sqlIdentifier(sequence_owner_column);
        for (const auto& column : side)
            out << ", " << sqlIdentifier(column.name);
        out << " FROM " << sqlIdentifier(table)
            << " ORDER BY " << sqlIdentifier(sequence_owner_column);
        for (const auto& column : collection.identity_columns())
            out << ", " << sqlIdentifier(column.name);
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
        for (const auto& field : layout.one_fields) {
            out << ",\n    " << sqlIdentifier(field.name) << ' ' << field.sqlType;
            if (!field.nullable) out << " NOT NULL";
        }
        out << "\n)";
        exec(db, out.str().c_str());
    }

    inline void create_collection_table(sqlite3* db, std::string_view table, const LayoutCollection& collection) {
        const auto identity = collection.identity_columns();
        std::ostringstream out;
        out << "CREATE TABLE " << sqlIdentifier(table) << " (\n"
            << "    " << sqlIdentifier(sequence_owner_column) << " INTEGER NOT NULL";
        for (const auto& column : identity) {
            out << ",\n    " << sqlIdentifier(column.name) << ' ' << column.sqlType;
            if (!column.nullable) out << " NOT NULL";
        }
        for (const auto& column : collection.value_columns) {
            out << ",\n    " << sqlIdentifier(column.name) << ' ' << column.sqlType;
            if (!column.nullable) out << " NOT NULL";
        }
        out << ",\n    PRIMARY KEY (" << sqlIdentifier(sequence_owner_column);
        for (const auto& column : identity)
            out << ", " << sqlIdentifier(column.name);
        out << ")\n)";
        exec(db, out.str().c_str());
    }

    template<typename Meta>
    void create_globals_table(sqlite3* db, const LayoutDesc<Meta>& layout) {
        std::ostringstream out;
        out << "CREATE TABLE " << sqlIdentifier(globals_table(layout.aspectName)) << " (\n"
            << "    key INTEGER PRIMARY KEY NOT NULL CHECK (key = 0)";
        for (const auto& field : layout.all_fields) {
            out << ",\n    " << sqlIdentifier(field.name) << ' ' << field.sqlType;
            if (!field.nullable) out << " NOT NULL";
        }
        out << "\n)";
        exec(db, out.str().c_str());
    }

    template<typename Meta>
    auto has_storage_schema(sqlite3* db) -> bool {
        const auto layout = layout_of<Meta>();
        if (!table_exists(db, quanta_table(layout.aspectName))) return false;
        for (const auto& field : layout.one_collections) {
            if (!table_exists(db, one_collection_table(layout.aspectName, field.relative_name)))
                return false;
        }
        for (const auto& field : layout.all_collections) {
            if (!table_exists(db, all_collection_table(layout.aspectName, field.relative_name)))
                return false;
        }
        if (!layout.all_fields.empty() && !table_exists(db, globals_table(layout.aspectName))) return false;
        return true;
    }

    template<typename Meta>
    void rewrite_tables(sqlite3* db) {
        const auto layout = layout_of<Meta>();

        for (const auto& field : layout.one_collections)
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(one_collection_table(layout.aspectName, field.relative_name))).c_str());
        for (const auto& field : layout.all_collections)
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(all_collection_table(layout.aspectName, field.relative_name))).c_str());
        exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(quanta_table(layout.aspectName))).c_str());
        if (!layout.all_fields.empty())
            exec(db, std::format("DROP TABLE IF EXISTS {}", sqlIdentifier(globals_table(layout.aspectName))).c_str());

        create_quanta_table(db, layout);
        for (const auto& field : layout.one_collections)
            create_collection_table(db, one_collection_table(layout.aspectName, field.relative_name), field);
        for (const auto& field : layout.all_collections)
            create_collection_table(db, all_collection_table(layout.aspectName, field.relative_name), field);
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
            using Leaf = std::decay_t<decltype(slot.get(quantum))>;
            if constexpr (json::detail::form::HasRetrospection<Leaf>)
                bindIndex = bind_product_fields(statement, bindIndex, slot.get(quantum));
            else
                bindIndex = bind_atom(statement, bindIndex, slot.get(quantum));
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename Meta>
    struct ReadOneFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        typename Meta::Quantum& quantum;
        int columnIndex = 1;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(quantum))>;
            if constexpr (json::detail::form::HasRetrospection<Leaf>)
                columnIndex = read_product_fields(statement, columnIndex, slot.get(quantum));
            else
                columnIndex = read_atom(statement, columnIndex, slot.get(quantum));
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename Meta>
    struct BindAllFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        const fqsm::GlobalValue<Meta>& global;
        int bindIndex = 1;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}

        template<auto... Members>
        void all(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(global))>;
            if constexpr (json::detail::form::HasRetrospection<Leaf>)
                bindIndex = bind_product_fields(statement, bindIndex, slot.get(global));
            else
                bindIndex = bind_atom(statement, bindIndex, slot.get(global));
        }

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename Meta>
    struct ReadAllFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        fqsm::GlobalValue<Meta>& global;
        int columnIndex = 0;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}

        template<auto... Members>
        void all(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(global))>;
            if constexpr (json::detail::form::HasRetrospection<Leaf>)
                columnIndex = read_product_fields(statement, columnIndex, slot.get(global));
            else
                columnIndex = read_atom(statement, columnIndex, slot.get(global));
        }

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename T>
    auto make_product() -> T {
        if constexpr (std::is_default_constructible_v<T>)
            return T{};
        else
            return fqsm::utility::BadValue{};
    }

    template<typename Key>
    auto key_as_int64(const Key& key) -> sqlite3_int64 {
        if constexpr (std::is_enum_v<Key>)
            return static_cast<sqlite3_int64>(static_cast<std::int32_t>(key));
        else if constexpr (requires { key.raw(); })
            return static_cast<sqlite3_int64>(key.raw());
        else
            return static_cast<sqlite3_int64>(key);
    }

    template<typename Product>
    struct SaveProductNestedDesc;

    template<typename Container>
    void save_container(
        sqlite3* db,
        std::string_view table,
        std::string_view aspect_root,
        bool under_all,
        std::string relative_name,
        std::string_view key_name,
        const std::vector<LayoutColumn>& parent_keys,
        const std::vector<sqlite3_int64>& parent_key_values,
        sqlite3_int64 owner_raw,
        const Container& container
    ) {
        std::vector<LayoutCollection> registered;
        register_collection_layout<typename Container::value_type, Container>(
            relative_name, key_name, parent_keys, registered);
        if (registered.empty())
            return;
        const LayoutCollection& level = registered.front();

        sqlite3_stmt* statement = nullptr;
        const auto sql = build_collection_insert_sql(table, level);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", table));

        if constexpr (MapContainer<Container>) {
            using Mapped = typename Container::mapped_type;
            for (const auto& entry : container) {
                sqlite3_reset(statement);
                sqlite3_clear_bindings(statement);
                int index = 1;
                sqlite3_bind_int64(statement, index++, owner_raw);
                for (const auto key_value : parent_key_values)
                    sqlite3_bind_int64(statement, index++, key_value);
                index = bind_atom(statement, index, entry.first);
                if constexpr (json::detail::form::HasRetrospection<Mapped>)
                    bind_product_fields(statement, index, entry.second);
                else
                    bind_atom(statement, index, entry.second);

                if (sqlite3_step(statement) != SQLITE_DONE) {
                    sqlite3_finalize(statement);
                    fail(db, std::format("insert {}", table));
                }

                if constexpr (json::detail::form::HasRetrospection<Mapped>) {
                    std::vector<sqlite3_int64> child_keys = parent_key_values;
                    child_keys.push_back(key_as_int64(entry.first));
                    SaveProductNestedDesc<Mapped> nested{
                        .db = db,
                        .aspect_root = aspect_root,
                        .under_all = under_all,
                        .parent_relative = relative_name,
                        .parent_key_columns = level.own_keys,
                        .parent_key_values = std::move(child_keys),
                        .owner_raw = owner_raw,
                        .product = entry.second,
                    };
                    fqsm::aspect::Retrospection<Mapped>::describe(nested);
                }
            }
        } else {
            std::size_t ordinal = 0;
            for (const auto& element : container) {
                using Elem = std::remove_cvref_t<decltype(element)>;
                sqlite3_reset(statement);
                sqlite3_clear_bindings(statement);
                int index = 1;
                sqlite3_bind_int64(statement, index++, owner_raw);
                for (const auto key_value : parent_key_values)
                    sqlite3_bind_int64(statement, index++, key_value);
                sqlite3_bind_int64(statement, index++, static_cast<sqlite3_int64>(ordinal));
                if constexpr (json::detail::form::HasRetrospection<Elem>)
                    bind_product_fields(statement, index, element);
                else
                    bind_atom(statement, index, element);

                if (sqlite3_step(statement) != SQLITE_DONE) {
                    sqlite3_finalize(statement);
                    fail(db, std::format("insert {}", table));
                }

                if constexpr (json::detail::form::HasRetrospection<Elem>) {
                    std::vector<LayoutColumn> ordinal_as_parent = parent_keys;
                    ordinal_as_parent.push_back(LayoutColumn{
                        .name = std::string{sequence_ordinal_column},
                        .sqlType = "INTEGER",
                        .nullable = false,
                    });
                    std::vector<sqlite3_int64> child_keys = parent_key_values;
                    child_keys.push_back(static_cast<sqlite3_int64>(ordinal));
                    SaveProductNestedDesc<Elem> nested{
                        .db = db,
                        .aspect_root = aspect_root,
                        .under_all = under_all,
                        .parent_relative = relative_name,
                        .parent_key_columns = std::move(ordinal_as_parent),
                        .parent_key_values = std::move(child_keys),
                        .owner_raw = owner_raw,
                        .product = element,
                    };
                    fqsm::aspect::Retrospection<Elem>::describe(nested);
                }

                ++ordinal;
            }
        }

        sqlite3_finalize(statement);
    }

    template<typename Product>
    struct SaveProductNestedDesc {
        sqlite3* db = nullptr;
        std::string_view aspect_root{};
        bool under_all = false;
        std::string parent_relative;
        std::vector<LayoutColumn> parent_key_columns;
        std::vector<sqlite3_int64> parent_key_values;
        sqlite3_int64 owner_raw = 0;
        const Product& product;

        void aspect(std::string_view) {}
        void all(auto&&) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(product))>;
            const auto relative = qualify(parent_relative, slot.name);
            const auto table = under_all
                ? all_collection_table(aspect_root, relative)
                : one_collection_table(aspect_root, relative);
            save_container(
                db,
                table,
                aspect_root,
                under_all,
                relative,
                slot.key_name,
                parent_key_columns,
                parent_key_values,
                owner_raw,
                slot.get(product)
            );
        }
    };

    template<typename Meta>
    struct SaveOneCollectionsDesc {
        sqlite3* db = nullptr;
        Reading context;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            assert_collection_elem<Elem, Container>();
            const auto table = one_collection_table(aspectName, slot.name);
            for (const auto entry : context->aspect<Meta>().items()) {
                save_container(
                    db,
                    table,
                    aspectName,
                    false,
                    std::string{slot.name},
                    slot.key_name,
                    {},
                    {},
                    static_cast<sqlite3_int64>(entry.id.raw()),
                    slot.get(entry.value)
                );
            }
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename Meta>
    struct SaveAllCollectionsDesc {
        sqlite3* db = nullptr;
        const fqsm::GlobalValue<Meta>& global;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(global))>;
            assert_collection_elem<Elem, Container>();
            save_container(
                db,
                all_collection_table(aspectName, slot.name),
                aspectName,
                true,
                std::string{slot.name},
                slot.key_name,
                {},
                {},
                sqlite3_int64{0},
                slot.get(global)
            );
        }
    };

    template<typename Container, typename Elem>
    void container_add(Container& container, Elem&& element) {
        if constexpr (requires { container.push_back(std::forward<Elem>(element)); })
            container.push_back(std::forward<Elem>(element));
        else
            container.insert(std::forward<Elem>(element));
    }

    template<typename Meta, typename ParentContainer, typename ParentSlot>
    struct LoadNestedUnderMapDesc {
        sqlite3* db = nullptr;
        Writing context;
        std::string_view aspect_root{};
        bool under_all = false;
        std::string parent_relative;
        std::vector<LayoutColumn> parent_key_columns;
        ParentSlot parent_slot;

        void aspect(std::string_view) {}
        void all(auto&&) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using ChildContainer = std::decay_t<decltype(slot.get(
                std::declval<typename ParentContainer::mapped_type&>()))>;
            const auto relative = qualify(parent_relative, slot.name);
            const auto table = under_all
                ? all_collection_table(aspect_root, relative)
                : one_collection_table(aspect_root, relative);

            std::vector<LayoutCollection> registered;
            register_collection_layout<Elem, ChildContainer>(
                relative, slot.key_name, parent_key_columns, registered);
            if (registered.empty())
                return;
            const LayoutCollection& level = registered.front();

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table, level);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                const auto owner = typename Meta::Id{
                    static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
                };
                int index = 1;
                using Key = typename ParentContainer::key_type;
                Key parent_key{};
                index = read_atom(statement, index, parent_key);
                if (level.sequence)
                    ++index;

                auto quantum = with<Meta>::modify(context, owner);
                auto& mapped = parent_slot.get(*quantum).at(parent_key);
                auto& child = slot.get(mapped);

                if constexpr (MapContainer<ChildContainer>) {
                    using ChildKey = typename ChildContainer::key_type;
                    using ChildMapped = typename ChildContainer::mapped_type;
                    ChildKey child_key{};
                    index = read_atom(statement, index, child_key);
                    ChildMapped child_mapped = make_product<ChildMapped>();
                    if constexpr (json::detail::form::HasRetrospection<ChildMapped>)
                        read_product_fields(statement, index, child_mapped);
                    else
                        read_atom(statement, index, child_mapped);
                    child.insert_or_assign(std::move(child_key), std::move(child_mapped));
                } else {
                    auto element = decode_element_fields<Elem>(statement, index);
                    container_add(child, std::move(element));
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

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            assert_collection_elem<Elem, Container>();

            const auto table = one_collection_table(aspectName, slot.name);
            std::vector<LayoutCollection> registered;
            register_collection_layout<Elem, Container>(std::string{slot.name}, slot.key_name, {}, registered);
            if (registered.empty())
                return;
            const LayoutCollection& level = registered.front();

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table, level);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                const auto owner = typename Meta::Id{
                    static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
                };
                auto quantum = with<Meta>::modify(context, owner);
                int index = 1;

                if constexpr (MapContainer<Container>) {
                    using Key = typename Container::key_type;
                    using Mapped = typename Container::mapped_type;
                    Key key{};
                    index = read_atom(statement, index, key);
                    Mapped mapped = make_product<Mapped>();
                    if constexpr (json::detail::form::HasRetrospection<Mapped>)
                        read_product_fields(statement, index, mapped);
                    else
                        read_atom(statement, index, mapped);
                    slot.get(*quantum).insert_or_assign(std::move(key), std::move(mapped));
                } else {
                    ++index;
                    auto element = decode_element_fields<Elem>(statement, index);
                    container_add(slot.get(*quantum), std::move(element));
                }
            }

            sqlite3_finalize(statement);

            if constexpr (MapContainer<Container>) {
                using Mapped = typename Container::mapped_type;
                if constexpr (json::detail::form::HasRetrospection<Mapped>) {
                    LoadNestedUnderMapDesc<Meta, Container, Collection<Elem, Members...>> nested{
                        .db = db,
                        .context = context,
                        .aspect_root = aspectName,
                        .under_all = false,
                        .parent_relative = std::string{slot.name},
                        .parent_key_columns = level.own_keys,
                        .parent_slot = slot,
                    };
                    fqsm::aspect::Retrospection<Mapped>::describe(nested);
                }
            }
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename Meta>
    struct LoadAllCollectionsDesc {
        sqlite3* db = nullptr;
        fqsm::GlobalValue<Meta>& global;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(global))>;
            assert_collection_elem<Elem, Container>();

            const auto table = all_collection_table(aspectName, slot.name);
            std::vector<LayoutCollection> registered;
            register_collection_layout<Elem, Container>(std::string{slot.name}, slot.key_name, {}, registered);
            if (registered.empty())
                return;
            const LayoutCollection& level = registered.front();

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table, level);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                int index = 2; // skip owner (0) — globals use owner 0; identity starts at 1
                if constexpr (MapContainer<Container>) {
                    using Key = typename Container::key_type;
                    using Mapped = typename Container::mapped_type;
                    Key key{};
                    index = read_atom(statement, 1, key);
                    Mapped mapped = make_product<Mapped>();
                    if constexpr (json::detail::form::HasRetrospection<Mapped>)
                        read_product_fields(statement, index, mapped);
                    else
                        read_atom(statement, index, mapped);
                    slot.get(global).insert_or_assign(std::move(key), std::move(mapped));
                } else {
                    auto element = decode_element_fields<Elem>(statement, 2);
                    container_add(slot.get(global), std::move(element));
                }
            }

            sqlite3_finalize(statement);
            (void)level;
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
            fqsm::aspect::Retrospection<Meta>::describe(binder);

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
        fqsm::aspect::Retrospection<Meta>::describe(desc);
    }

    template<typename Meta>
    void save_global_collections(sqlite3* db, Reading context) {
        const auto layout = layout_of<Meta>();
        if (layout.all_collections.empty()) return;

        const auto& global = with<Meta>::get_global(context);
        SaveAllCollectionsDesc<Meta> desc{db, global, {}};
        fqsm::aspect::Retrospection<Meta>::describe(desc);
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
        fqsm::aspect::Retrospection<Meta>::describe(binder);

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
            if (!with<Meta>::exists(context, id))
                context.workers_interface().updates<Meta>().put_modification(id, fqsm::utility::BadValue{});
            auto quantum = with<Meta>::modify(context, id);

            ReadOneFieldsDesc<Meta> reader{statement, *quantum, 1};
            fqsm::aspect::Retrospection<Meta>::describe(reader);
        }

        sqlite3_finalize(statement);
    }

    template<typename Meta>
    void load_collections(sqlite3* db, Writing context) {
        LoadOneCollectionsDesc<Meta> desc{db, context, {}};
        fqsm::aspect::Retrospection<Meta>::describe(desc);
    }

    template<typename Meta>
    void load_global_collections(sqlite3* db, Writing context) {
        const auto layout = layout_of<Meta>();
        if (layout.all_collections.empty()) return;

        auto global = with<Meta>::modify_global(context);
        LoadAllCollectionsDesc<Meta> desc{db, *global, {}};
        fqsm::aspect::Retrospection<Meta>::describe(desc);
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
            fqsm::aspect::Retrospection<Meta>::describe(reader);
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
    auto quanta_count(sqlite3* db, const LayoutDesc<Meta>& layout) -> std::size_t {
        sqlite3_stmt* statement = nullptr;
        const auto sql = std::format(
            "SELECT COUNT(*) FROM {}",
            sqlIdentifier(quanta_table(layout.aspectName))
        );
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare count {}", quanta_table(layout.aspectName)));
        std::size_t count = 0;
        if (sqlite3_step(statement) == SQLITE_ROW)
            count = static_cast<std::size_t>(sqlite3_column_int64(statement, 0));
        sqlite3_finalize(statement);
        return count;
    }

    template<typename Meta>
    void replace_quanta(sqlite3* db, Direct<Meta> gate) {
        const auto layout = layout_of<Meta>();
        gate.items.clear();
        gate.items.reserve(quanta_count<Meta>(db, layout));

        sqlite3_stmt* statement = nullptr;
        const auto sql = build_quanta_select_sql(layout);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", quanta_table(layout.aspectName)));

        while (sqlite3_step(statement) == SQLITE_ROW) {
            const auto id = typename Meta::Id{
                static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
            };
            typename Meta::Quantum quantum = fqsm::utility::BadValue{};
            ReadOneFieldsDesc<Meta> reader{statement, quantum, 1};
            fqsm::aspect::Retrospection<Meta>::describe(reader);
            gate.items.insert(id, std::move(quantum));
        }

        sqlite3_finalize(statement);
    }

    template<typename Meta, typename ParentContainer, typename ParentSlot>
    void replace_nested_under_map(
        sqlite3* db,
        typename Direct<Meta>::Container& items,
        std::string_view aspect_root,
        std::string parent_relative,
        const std::vector<LayoutColumn>& parent_key_columns,
        ParentSlot parent_slot
    );

    template<typename Meta>
    struct ReplaceOneCollectionsDesc {
        sqlite3* db = nullptr;
        typename Direct<Meta>::Container& items;
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
            assert_collection_elem<Elem, Container>();

            const auto table = one_collection_table(aspectName, slot.name);
            std::vector<LayoutCollection> registered;
            register_collection_layout<Elem, Container>(std::string{slot.name}, slot.key_name, {}, registered);
            if (registered.empty())
                return;
            const LayoutCollection& level = registered.front();

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table, level);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                const auto owner = typename Meta::Id{
                    static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
                };
                auto* quantum = items.find(owner);
                if (!quantum)
                    fail(db, std::format("replace collection: missing owner in {}", table));
                int index = 1;

                if constexpr (MapContainer<Container>) {
                    using Key = typename Container::key_type;
                    using Mapped = typename Container::mapped_type;
                    Key key{};
                    index = read_atom(statement, index, key);
                    Mapped mapped = make_product<Mapped>();
                    if constexpr (json::detail::form::HasRetrospection<Mapped>)
                        read_product_fields(statement, index, mapped);
                    else
                        read_atom(statement, index, mapped);
                    slot.get(*quantum).insert_or_assign(std::move(key), std::move(mapped));
                } else {
                    ++index;
                    auto element = decode_element_fields<Elem>(statement, index);
                    container_add(slot.get(*quantum), std::move(element));
                }
            }

            sqlite3_finalize(statement);

            if constexpr (MapContainer<Container>) {
                using Mapped = typename Container::mapped_type;
                if constexpr (json::detail::form::HasRetrospection<Mapped>) {
                    replace_nested_under_map<Meta, Container, Collection<Elem, Members...>>(
                        db, items, aspectName, std::string{slot.name}, level.own_keys, slot);
                }
            }
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void all(Collection<Elem, Members...>) {}
    };

    template<typename Meta, typename ParentContainer, typename ParentSlot>
    struct ReplaceNestedUnderMapDesc {
        sqlite3* db = nullptr;
        typename Direct<Meta>::Container& items;
        std::string_view aspect_root{};
        std::string parent_relative;
        std::vector<LayoutColumn> parent_key_columns;
        ParentSlot parent_slot;

        void aspect(std::string_view) {}
        void all(auto&&) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            using ChildContainer = std::decay_t<decltype(slot.get(
                std::declval<typename ParentContainer::mapped_type&>()))>;
            const auto relative = qualify(parent_relative, slot.name);
            const auto table = one_collection_table(aspect_root, relative);

            std::vector<LayoutCollection> registered;
            register_collection_layout<Elem, ChildContainer>(
                relative, slot.key_name, parent_key_columns, registered);
            if (registered.empty())
                return;
            const LayoutCollection& level = registered.front();

            sqlite3_stmt* statement = nullptr;
            const auto sql = build_collection_select_sql(table, level);
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
                fail(db, std::format("prepare {}", table));

            while (sqlite3_step(statement) == SQLITE_ROW) {
                const auto owner = typename Meta::Id{
                    static_cast<typename Meta::Id::Raw>(sqlite3_column_int64(statement, 0))
                };
                auto* quantum = items.find(owner);
                if (!quantum)
                    fail(db, std::format("replace nested: missing owner in {}", table));

                int index = 1;
                using Key = typename ParentContainer::key_type;
                Key parent_key{};
                index = read_atom(statement, index, parent_key);
                if (level.sequence)
                    ++index;

                auto& mapped = parent_slot.get(*quantum).at(parent_key);
                auto& child = slot.get(mapped);
                auto element = decode_element_fields<Elem>(statement, index);
                container_add(child, std::move(element));
            }

            sqlite3_finalize(statement);
        }
    };

    template<typename Meta, typename ParentContainer, typename ParentSlot>
    void replace_nested_under_map(
        sqlite3* db,
        typename Direct<Meta>::Container& items,
        std::string_view aspect_root,
        std::string parent_relative,
        const std::vector<LayoutColumn>& parent_key_columns,
        ParentSlot parent_slot
    ) {
        using Mapped = typename ParentContainer::mapped_type;
        ReplaceNestedUnderMapDesc<Meta, ParentContainer, ParentSlot> desc{
            .db = db,
            .items = items,
            .aspect_root = aspect_root,
            .parent_relative = std::move(parent_relative),
            .parent_key_columns = parent_key_columns,
            .parent_slot = parent_slot,
        };
        fqsm::aspect::Retrospection<Mapped>::describe(desc);
    }

    template<typename Meta>
    void replace_collections(sqlite3* db, Direct<Meta> gate) {
        ReplaceOneCollectionsDesc<Meta> desc{db, gate.items, {}};
        fqsm::aspect::Retrospection<Meta>::describe(desc);
    }

    template<typename Meta>
    void replace_globals(sqlite3* db, Direct<Meta> gate) {
        const auto layout = layout_of<Meta>();
        if (layout.all_fields.empty()) return;

        sqlite3_stmt* statement = nullptr;
        const auto sql = build_globals_select_sql(layout);
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
            fail(db, std::format("prepare {}", globals_table(layout.aspectName)));

        if (sqlite3_step(statement) == SQLITE_ROW) {
            ReadAllFieldsDesc<Meta> reader{statement, gate.global, 0};
            fqsm::aspect::Retrospection<Meta>::describe(reader);
        }

        sqlite3_finalize(statement);
    }

    template<typename Meta>
    void replace_global_collections(sqlite3* db, Direct<Meta> gate) {
        const auto layout = layout_of<Meta>();
        if (layout.all_collections.empty()) return;

        LoadAllCollectionsDesc<Meta> desc{db, gate.global, {}};
        fqsm::aspect::Retrospection<Meta>::describe(desc);
    }

    template<typename Meta>
    void replace_aspect(sqlite3* db, Direct<Meta> gate) {
        replace_quanta<Meta>(db, gate);
        replace_collections<Meta>(db, gate);
        gate.global = {};
        replace_globals<Meta>(db, gate);
        replace_global_collections<Meta>(db, gate);
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

        void replace(Writing context, DatabaseProxy& db) override {
            pull(context, db);
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
        return persistency::aspect<Meta>(std::shared_ptr<Archive>{ArchiveOps::of<Meta>()});
    }

    template<fqsm::meta::category::Any Meta>
        requires HasRetrospection<Meta>
    struct Aspect {
        operator Schema() const { return aspect<Meta>(); }
    };

}
