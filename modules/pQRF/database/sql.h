#pragma once

#include <cstdint>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <type_traits>

#include <fQSM/identifier.h>

namespace fqsm::processing::persistency::database::detail::sql {

    template<typename T>
    inline constexpr bool always_false = false;

    template<typename T>
    concept forbidden_sql_type =
        std::is_pointer_v<T>
        || std::is_reference_v<T>
        || std::is_function_v<T>
        || std::is_member_pointer_v<T>;

    template<typename T>
    consteval void reject_if_forbidden() {
        static_assert(!forbidden_sql_type<T>, "SQL persistence forbids pointers/references/functions/member-pointers");
    }

    template<typename T>
    struct atom {
        static constexpr bool nullable = false;

        static consteval void require() {
            reject_if_forbidden<T>();
            static_assert(always_false<T>, "Missing SQL atom policy specialization for this type");
        }
    };

    template<>
    struct atom<std::string> {
        static constexpr bool nullable = false;
        static constexpr std::string_view sql_name = "TEXT";

        static void bind(sqlite3_stmt* statement, int index, const std::string& value) {
            sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::string {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, index));
            return text ? text : "";
        }

        static void read(sqlite3_stmt* statement, int index, std::string& value) {
            value = decode(statement, index);
        }

        static consteval void require() {}
    };

    template<>
    struct atom<std::int32_t> {
        static constexpr bool nullable = false;
        static constexpr std::string_view sql_name = "INTEGER";

        static void bind(sqlite3_stmt* statement, int index, const std::int32_t& value) {
            sqlite3_bind_int(statement, index, value);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::int32_t {
            return static_cast<std::int32_t>(sqlite3_column_int(statement, index));
        }

        static void read(sqlite3_stmt* statement, int index, std::int32_t& value) {
            value = decode(statement, index);
        }

        static consteval void require() {}
    };

    template<>
    struct atom<bool> {
        static constexpr bool nullable = false;
        static constexpr std::string_view sql_name = "INTEGER";

        static void bind(sqlite3_stmt* statement, int index, const bool& value) {
            sqlite3_bind_int(statement, index, value ? 1 : 0);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> bool {
            return sqlite3_column_int(statement, index) != 0;
        }

        static void read(sqlite3_stmt* statement, int index, bool& value) {
            value = decode(statement, index);
        }

        static consteval void require() {}
    };

    template<typename Meta, typename BaseType>
    struct atom<fqsm::Identifier<Meta, BaseType>> {
        using Id = fqsm::Identifier<Meta, BaseType>;

        static constexpr bool nullable = false;
        static constexpr std::string_view sql_name = "INTEGER";

        static void bind(sqlite3_stmt* statement, int index, const Id& value) {
            sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value.raw()));
        }

        static auto decode(sqlite3_stmt* statement, int index) -> Id {
            return Id{static_cast<BaseType>(sqlite3_column_int64(statement, index))};
        }

        static void read(sqlite3_stmt* statement, int index, Id& value) {
            value = decode(statement, index);
        }

        static consteval void require() {}
    };

    template<typename T>
    struct atom<std::optional<T>> {
        static constexpr bool nullable = true;
        static constexpr std::string_view sql_name = atom<T>::sql_name;

        static void bind(sqlite3_stmt* statement, int index, const std::optional<T>& value) {
            if (!value.has_value()) {
                sqlite3_bind_null(statement, index);
                return;
            }
            atom<T>::bind(statement, index, *value);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::optional<T> {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL)
                return std::nullopt;
            return atom<T>::decode(statement, index);
        }

        static void read(sqlite3_stmt* statement, int index, std::optional<T>& value) {
            value = decode(statement, index);
        }

        static consteval void require() {
            atom<T>::require();
        }
    };

    template<typename T>
    void bind(sqlite3_stmt* statement, int index, const T& value) {
        atom<T>::require();
        atom<T>::bind(statement, index, value);
    }

    template<typename T>
    void read(sqlite3_stmt* statement, int index, T& value) {
        atom<T>::require();
        atom<T>::read(statement, index, value);
    }

    template<typename T>
    auto decode(sqlite3_stmt* statement, int index) -> T {
        atom<T>::require();
        return atom<T>::decode(statement, index);
    }

}
