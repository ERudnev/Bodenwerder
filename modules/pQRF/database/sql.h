#pragma once

// SQL leaf / composite atoms: one describe slot → one or more columns in the same row.
// Containers are not atoms — collection<> walks elements and uses atom<Elem>.

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <type_traits>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <base/maybe.h>
#include <base/types/common_types.h>
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

    struct ColumnDef {
        std::string_view suffix;   // "" or "_x" / "_y" / …
        std::string_view sql_type; // INTEGER / REAL / TEXT / BLOB
    };

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
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "TEXT"}};

        static auto bind(sqlite3_stmt* statement, int index, const std::string& value) -> int {
            sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, std::string& value) -> int {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, index));
            value = text ? text : "";
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::string {
            std::string value;
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<std::int32_t> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "INTEGER"}};

        static auto bind(sqlite3_stmt* statement, int index, const std::int32_t& value) -> int {
            sqlite3_bind_int(statement, index, value);
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, std::int32_t& value) -> int {
            value = static_cast<std::int32_t>(sqlite3_column_int(statement, index));
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::int32_t {
            std::int32_t value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<bool> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "INTEGER"}};

        static auto bind(sqlite3_stmt* statement, int index, const bool& value) -> int {
            sqlite3_bind_int(statement, index, value ? 1 : 0);
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, bool& value) -> int {
            value = sqlite3_column_int(statement, index) != 0;
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> bool {
            bool value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<float> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "REAL"}};

        static auto bind(sqlite3_stmt* statement, int index, const float& value) -> int {
            sqlite3_bind_double(statement, index, static_cast<double>(value));
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, float& value) -> int {
            value = static_cast<float>(sqlite3_column_double(statement, index));
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> float {
            float value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<double> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "REAL"}};

        static auto bind(sqlite3_stmt* statement, int index, const double& value) -> int {
            sqlite3_bind_double(statement, index, value);
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, double& value) -> int {
            value = sqlite3_column_double(statement, index);
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> double {
            double value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<std::filesystem::path> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "TEXT"}};

        static auto bind(sqlite3_stmt* statement, int index, const std::filesystem::path& value) -> int {
            const auto text = value.generic_string();
            sqlite3_bind_text(statement, index, text.c_str(), -1, SQLITE_TRANSIENT);
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, std::filesystem::path& value) -> int {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, index));
            value = std::filesystem::path{text ? text : ""};
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::filesystem::path {
            std::filesystem::path value;
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<std::chrono::system_clock::time_point> {
        using Timepoint = std::chrono::system_clock::time_point;

        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "INTEGER"}};

        static auto bind(sqlite3_stmt* statement, int index, const Timepoint& value) -> int {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();
            sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(ms));
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, Timepoint& value) -> int {
            value = Timepoint{std::chrono::milliseconds{sqlite3_column_int64(statement, index)}};
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> Timepoint {
            Timepoint value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<typename Meta, typename BaseType>
    struct atom<fqsm::Identifier<Meta, BaseType>> {
        using Id = fqsm::Identifier<Meta, BaseType>;

        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "INTEGER"}};

        static auto bind(sqlite3_stmt* statement, int index, const Id& value) -> int {
            sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value.raw()));
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, Id& value) -> int {
            value = decode(statement, index);
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> Id {
            return Id{static_cast<BaseType>(sqlite3_column_int64(statement, index))};
        }

        static consteval void require() {}
    };

    template<typename T>
        requires std::is_enum_v<T>
    struct atom<T> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "INTEGER"}};

        static auto bind(sqlite3_stmt* statement, int index, const T& value) -> int {
            return atom<std::int32_t>::bind(statement, index, static_cast<std::int32_t>(value));
        }

        static auto read(sqlite3_stmt* statement, int index, T& value) -> int {
            std::int32_t raw = 0;
            const auto next = atom<std::int32_t>::read(statement, index, raw);
            value = static_cast<T>(raw);
            return next;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> T {
            T value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<base::common_types::index2> {
        using index2 = base::common_types::index2;

        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 2>{
            ColumnDef{"_x", "INTEGER"},
            ColumnDef{"_y", "INTEGER"},
        };

        static auto bind(sqlite3_stmt* statement, int index, const index2& value) -> int {
            index = atom<std::int32_t>::bind(statement, index, value.x);
            return atom<std::int32_t>::bind(statement, index, value.y);
        }

        static auto read(sqlite3_stmt* statement, int index, index2& value) -> int {
            index = atom<std::int32_t>::read(statement, index, value.x);
            return atom<std::int32_t>::read(statement, index, value.y);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> index2 {
            index2 value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<base::common_types::index3> {
        using index3 = base::common_types::index3;

        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 3>{
            ColumnDef{"_x", "INTEGER"},
            ColumnDef{"_y", "INTEGER"},
            ColumnDef{"_z", "INTEGER"},
        };

        static auto bind(sqlite3_stmt* statement, int index, const index3& value) -> int {
            index = atom<std::int32_t>::bind(statement, index, value.x);
            index = atom<std::int32_t>::bind(statement, index, value.y);
            return atom<std::int32_t>::bind(statement, index, value.z);
        }

        static auto read(sqlite3_stmt* statement, int index, index3& value) -> int {
            index = atom<std::int32_t>::read(statement, index, value.x);
            index = atom<std::int32_t>::read(statement, index, value.y);
            return atom<std::int32_t>::read(statement, index, value.z);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> index3 {
            index3 value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<glm::vec2> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 2>{
            ColumnDef{"_x", "REAL"},
            ColumnDef{"_y", "REAL"},
        };

        static auto bind(sqlite3_stmt* statement, int index, const glm::vec2& value) -> int {
            index = atom<float>::bind(statement, index, value.x);
            return atom<float>::bind(statement, index, value.y);
        }

        static auto read(sqlite3_stmt* statement, int index, glm::vec2& value) -> int {
            index = atom<float>::read(statement, index, value.x);
            return atom<float>::read(statement, index, value.y);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> glm::vec2 {
            glm::vec2 value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<glm::vec3> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 3>{
            ColumnDef{"_x", "REAL"},
            ColumnDef{"_y", "REAL"},
            ColumnDef{"_z", "REAL"},
        };

        static auto bind(sqlite3_stmt* statement, int index, const glm::vec3& value) -> int {
            index = atom<float>::bind(statement, index, value.x);
            index = atom<float>::bind(statement, index, value.y);
            return atom<float>::bind(statement, index, value.z);
        }

        static auto read(sqlite3_stmt* statement, int index, glm::vec3& value) -> int {
            index = atom<float>::read(statement, index, value.x);
            index = atom<float>::read(statement, index, value.y);
            return atom<float>::read(statement, index, value.z);
        }

        static auto decode(sqlite3_stmt* statement, int index) -> glm::vec3 {
            glm::vec3 value{};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<>
    struct atom<glm::mat4> {
        static constexpr bool nullable = false;
        static constexpr auto columns = std::array<ColumnDef, 1>{ColumnDef{"", "BLOB"}};

        static auto bind(sqlite3_stmt* statement, int index, const glm::mat4& value) -> int {
            sqlite3_bind_blob(statement, index, &value[0][0], static_cast<int>(16 * sizeof(float)), SQLITE_TRANSIENT);
            return index + 1;
        }

        static auto read(sqlite3_stmt* statement, int index, glm::mat4& value) -> int {
            const auto* bytes = sqlite3_column_blob(statement, index);
            const auto size = sqlite3_column_bytes(statement, index);
            if (!bytes || size != static_cast<int>(16 * sizeof(float)))
                value = glm::mat4{1.0f};
            else
                std::memcpy(&value[0][0], bytes, 16 * sizeof(float));
            return index + 1;
        }

        static auto decode(sqlite3_stmt* statement, int index) -> glm::mat4 {
            glm::mat4 value{1.0f};
            read(statement, index, value);
            return value;
        }

        static consteval void require() {}
    };

    template<typename T>
    struct atom<std::optional<T>> {
        static constexpr bool nullable = true;
        static constexpr auto columns = atom<T>::columns;

        static auto bind(sqlite3_stmt* statement, int index, const std::optional<T>& value) -> int {
            if (!value.has_value()) {
                for (std::size_t i = 0; i < columns.size(); ++i)
                    sqlite3_bind_null(statement, index + static_cast<int>(i));
                return index + static_cast<int>(columns.size());
            }
            return atom<T>::bind(statement, index, *value);
        }

        static auto read(sqlite3_stmt* statement, int index, std::optional<T>& value) -> int {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                value = std::nullopt;
                return index + static_cast<int>(columns.size());
            }
            value = atom<T>::decode(statement, index);
            return index + static_cast<int>(columns.size());
        }

        static auto decode(sqlite3_stmt* statement, int index) -> std::optional<T> {
            std::optional<T> value;
            read(statement, index, value);
            return value;
        }

        static consteval void require() {
            atom<T>::require();
        }
    };

    template<typename T>
    struct atom<base::maybe<T>> {
        static constexpr bool nullable = true;
        static constexpr auto columns = atom<T>::columns;

        static auto bind(sqlite3_stmt* statement, int index, const base::maybe<T>& value) -> int {
            if (!value.exists()) {
                for (std::size_t i = 0; i < columns.size(); ++i)
                    sqlite3_bind_null(statement, index + static_cast<int>(i));
                return index + static_cast<int>(columns.size());
            }
            return atom<T>::bind(statement, index, *value);
        }

        static auto read(sqlite3_stmt* statement, int index, base::maybe<T>& value) -> int {
            if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
                value = std::nullopt;
                return index + static_cast<int>(columns.size());
            }
            value = atom<T>::decode(statement, index);
            return index + static_cast<int>(columns.size());
        }

        static auto decode(sqlite3_stmt* statement, int index) -> base::maybe<T> {
            base::maybe<T> value;
            read(statement, index, value);
            return value;
        }

        static consteval void require() {
            atom<T>::require();
        }
    };

    template<typename T>
    auto bind(sqlite3_stmt* statement, int index, const T& value) -> int {
        atom<T>::require();
        return atom<T>::bind(statement, index, value);
    }

    template<typename T>
    auto read(sqlite3_stmt* statement, int index, T& value) -> int {
        atom<T>::require();
        return atom<T>::read(statement, index, value);
    }

    template<typename T>
    auto decode(sqlite3_stmt* statement, int index) -> T {
        atom<T>::require();
        return atom<T>::decode(statement, index);
    }

}
