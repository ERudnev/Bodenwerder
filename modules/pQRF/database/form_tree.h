#pragma once

// Relational form tree: nested collection<> → child tables; nested Retrospection fields → dotted columns.
// Map collections: PK (owner, key…); sequences: PK (owner, parent keys…, ordinal).

#include <fQSM/aspect/persistency.h>
#include <fQSM/meta/retrospection.h>
#include <fQSM/utility/bad_value.h>
#include <pQRF/database/sql.h>
#include <pQRF/json/value_form.h>

#include <sqlite3.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fqsm::processing::persistency::database::detail {

    using fqsm::aspect::Collection;
    using fqsm::aspect::Field;

    constexpr std::string_view sequence_owner_column = "owner";
    constexpr std::string_view sequence_ordinal_column = "ordinal";
    constexpr std::string_view sequence_value_base = "value";

    inline auto qualify(std::string_view lhs, std::string_view rhs) -> std::string {
        return std::string{lhs} + "." + std::string{rhs};
    }

    struct LayoutColumn {
        std::string name;
        std::string_view sqlType;
        bool nullable = false;
    };

    struct LayoutCollection {
        std::string relative_name;
        std::vector<LayoutColumn> parent_keys;
        std::vector<LayoutColumn> own_keys;
        bool sequence = false;
        std::vector<LayoutColumn> value_columns;

        auto identity_columns() const -> std::vector<LayoutColumn> {
            std::vector<LayoutColumn> columns = parent_keys;
            columns.insert(columns.end(), own_keys.begin(), own_keys.end());
            if (sequence) {
                columns.push_back(LayoutColumn{
                    .name = std::string{sequence_ordinal_column},
                    .sqlType = "INTEGER",
                    .nullable = false,
                });
            }
            return columns;
        }

        auto all_value_side_columns() const -> std::vector<LayoutColumn> {
            auto columns = identity_columns();
            columns.insert(columns.end(), value_columns.begin(), value_columns.end());
            return columns;
        }
    };

    template<typename Leaf>
    concept HasSqlAtomColumns = requires {
        sql::atom<Leaf>::columns;
    };

    template<typename Leaf>
    constexpr auto leaf_nullable() -> bool {
        if constexpr (HasSqlAtomColumns<Leaf>)
            return sql::atom<Leaf>::nullable;
        else
            return false;
    }

    template<typename Container>
    concept MapContainer = requires { typename Container::mapped_type; };

    template<typename Leaf>
    void expand_atom_columns(std::string_view base_name, bool nullable, std::vector<LayoutColumn>& out) {
        using Bare = std::remove_cvref_t<Leaf>;
        static_assert(HasSqlAtomColumns<Bare>, "DB leaf must be a SQL atom");
        sql::atom<Bare>::require();
        for (const auto& column : sql::atom<Bare>::columns) {
            out.push_back(LayoutColumn{
                .name = std::string{base_name} + std::string{column.suffix},
                .sqlType = column.sql_type,
                .nullable = nullable,
            });
        }
    }

    template<typename Leaf>
    auto bind_atom(sqlite3_stmt* statement, int index, const Leaf& value) -> int {
        return sql::bind(statement, index, value);
    }

    template<typename Leaf>
    auto read_atom(sqlite3_stmt* statement, int index, Leaf& value) -> int {
        return sql::read(statement, index, value);
    }

    template<typename Root>
    struct FlattenProductDesc;

    template<typename Elem, typename Container>
    void register_collection_layout(
        std::string relative_name,
        std::string_view key_name,
        const std::vector<LayoutColumn>& parent_keys,
        std::vector<LayoutCollection>& out
    );

    template<typename Leaf>
    void append_product_or_atom(std::string_view path, std::vector<LayoutColumn>& out) {
        using Bare = std::remove_cvref_t<Leaf>;
        if constexpr (json::detail::form::HasRetrospection<Bare>) {
            FlattenProductDesc<Bare> flatten{
                .prefix = std::string{path},
                .value_columns = out,
                .nested = nullptr,
                .parent_table_path = {},
                .keys_for_nested = {},
            };
            fqsm::aspect::Retrospection<Bare>::describe(flatten);
        } else {
            expand_atom_columns<Bare>(path, leaf_nullable<Bare>(), out);
        }
    }

    template<typename Root>
    struct FlattenProductDesc {
        std::string prefix;
        std::vector<LayoutColumn>& value_columns;
        std::vector<LayoutCollection>* nested = nullptr;
        std::string parent_table_path;
        std::vector<LayoutColumn> keys_for_nested;

        void aspect(std::string_view) {}
        void all(auto&&) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(std::declval<Root&>()))>;
            const auto path = prefix.empty() ? std::string{slot.name} : qualify(prefix, slot.name);
            append_product_or_atom<Leaf>(path, value_columns);
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            if (nested == nullptr)
                return;
            using Container = std::decay_t<decltype(slot.get(std::declval<Root&>()))>;
            register_collection_layout<Elem, Container>(
                qualify(parent_table_path, slot.name),
                slot.key_name,
                keys_for_nested,
                *nested
            );
        }
    };

    template<typename Elem, typename Container>
    void register_collection_layout(
        std::string relative_name,
        std::string_view key_name,
        const std::vector<LayoutColumn>& parent_keys,
        std::vector<LayoutCollection>& out
    ) {
        LayoutCollection table{
            .relative_name = std::move(relative_name),
            .parent_keys = parent_keys,
            .own_keys = {},
            .sequence = false,
            .value_columns = {},
        };

        if constexpr (MapContainer<Container>) {
            using Key = typename Container::key_type;
            using Mapped = typename Container::mapped_type;
            expand_atom_columns<Key>(key_name, leaf_nullable<Key>(), table.own_keys);

            std::vector<LayoutColumn> child_keys = parent_keys;
            child_keys.insert(child_keys.end(), table.own_keys.begin(), table.own_keys.end());

            std::vector<LayoutCollection> nested;
            if constexpr (json::detail::form::HasRetrospection<Mapped>) {
                FlattenProductDesc<Mapped> flatten{
                    .prefix = {},
                    .value_columns = table.value_columns,
                    .nested = &nested,
                    .parent_table_path = table.relative_name,
                    .keys_for_nested = child_keys,
                };
                fqsm::aspect::Retrospection<Mapped>::describe(flatten);
            } else {
                expand_atom_columns<Mapped>(sequence_value_base, leaf_nullable<Mapped>(), table.value_columns);
            }

            out.push_back(std::move(table));
            for (auto& child : nested)
                out.push_back(std::move(child));
        } else {
            table.sequence = true;
            std::vector<LayoutColumn> child_keys = parent_keys;
            child_keys.push_back(LayoutColumn{
                .name = std::string{sequence_ordinal_column},
                .sqlType = "INTEGER",
                .nullable = false,
            });

            std::vector<LayoutCollection> nested;
            if constexpr (json::detail::form::HasRetrospection<Elem>) {
                FlattenProductDesc<Elem> flatten{
                    .prefix = {},
                    .value_columns = table.value_columns,
                    .nested = &nested,
                    .parent_table_path = table.relative_name,
                    .keys_for_nested = child_keys,
                };
                fqsm::aspect::Retrospection<Elem>::describe(flatten);
            } else if constexpr (json::detail::form::is_pair<std::remove_cvref_t<Elem>>::value) {
                expand_atom_columns<typename Elem::first_type>(
                    key_name, leaf_nullable<typename Elem::first_type>(), table.value_columns);
                using Second = typename Elem::second_type;
                if constexpr (json::detail::form::HasRetrospection<Second>) {
                    FlattenProductDesc<Second> flatten{
                        .prefix = {},
                        .value_columns = table.value_columns,
                        .nested = &nested,
                        .parent_table_path = table.relative_name,
                        .keys_for_nested = child_keys,
                    };
                    fqsm::aspect::Retrospection<Second>::describe(flatten);
                } else {
                    expand_atom_columns<Second>(sequence_value_base, leaf_nullable<Second>(), table.value_columns);
                }
            } else {
                expand_atom_columns<Elem>(sequence_value_base, leaf_nullable<Elem>(), table.value_columns);
            }

            out.push_back(std::move(table));
            for (auto& child : nested)
                out.push_back(std::move(child));
        }
    }

    // --- bind / read product fields (no collections) ---

    template<typename Root>
    struct BindProductFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        const Root& root;
        int index = 0;

        void aspect(std::string_view) {}
        void all(auto&&) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(root))>;
            if constexpr (json::detail::form::HasRetrospection<Leaf>) {
                BindProductFieldsDesc<Leaf> inner{statement, slot.get(root), index};
                fqsm::aspect::Retrospection<Leaf>::describe(inner);
                index = inner.index;
            } else {
                index = bind_atom(statement, index, slot.get(root));
            }
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}
    };

    template<typename Root>
    struct ReadProductFieldsDesc {
        sqlite3_stmt* statement = nullptr;
        Root& root;
        int index = 0;

        void aspect(std::string_view) {}
        void all(auto&&) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            using Leaf = std::decay_t<decltype(slot.get(root))>;
            if constexpr (json::detail::form::HasRetrospection<Leaf>) {
                ReadProductFieldsDesc<Leaf> inner{statement, slot.get(root), index};
                fqsm::aspect::Retrospection<Leaf>::describe(inner);
                index = inner.index;
            } else {
                index = read_atom(statement, index, slot.get(root));
            }
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...>) {}
    };

    template<typename Root>
    auto bind_product_fields(sqlite3_stmt* statement, int index, const Root& root) -> int {
        if constexpr (json::detail::form::HasRetrospection<Root>) {
            BindProductFieldsDesc<Root> binder{statement, root, index};
            fqsm::aspect::Retrospection<Root>::describe(binder);
            return binder.index;
        } else {
            return bind_atom(statement, index, root);
        }
    }

    template<typename Root>
    auto read_product_fields(sqlite3_stmt* statement, int index, Root& root) -> int {
        if constexpr (json::detail::form::HasRetrospection<Root>) {
            ReadProductFieldsDesc<Root> reader{statement, root, index};
            fqsm::aspect::Retrospection<Root>::describe(reader);
            return reader.index;
        } else {
            return read_atom(statement, index, root);
        }
    }

    // Construct map/sequence element from columns (collections left empty).
    // Products with Id: restore shell (same as quanta put_as_restored).
    template<typename Elem>
    auto decode_element_fields(sqlite3_stmt* statement, int index) -> Elem {
        using Bare = std::remove_cvref_t<Elem>;
        if constexpr (json::detail::form::is_pair<Bare>::value) {
            using First = std::remove_cv_t<typename Bare::first_type>;
            using Second = typename Bare::second_type;
            First first{};
            index = read_atom(statement, index, first);
            Second second = fqsm::utility::BadValue{};
            if constexpr (json::detail::form::HasRetrospection<Second>)
                read_product_fields(statement, index, second);
            else
                read_atom(statement, index, second);
            return Bare{std::move(first), std::move(second)};
        } else if constexpr (json::detail::form::HasRetrospection<Bare>) {
            Bare value = fqsm::utility::BadValue{};
            read_product_fields(statement, index, value);
            return value;
        } else {
            Bare value{};
            read_atom(statement, index, value);
            return value;
        }
    }

    template<typename Elem, typename Container>
    consteval void assert_collection_elem() {
        if constexpr (MapContainer<Container>) {
            static_assert(
                std::is_same_v<Elem, typename Container::value_type>
                    || std::is_same_v<Elem, std::pair<typename Container::key_type, typename Container::mapped_type>>,
                "collection Elem must match map value_type or pair<key, mapped>");
        } else {
            static_assert(std::is_same_v<Elem, typename Container::value_type>,
                "collection Elem must match container value_type");
        }
    }

}
