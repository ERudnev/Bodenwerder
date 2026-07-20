#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <type_traits>
#include <vector>

#include <base/maybe.h>
#include <fQSM/identifier.h>

namespace placeholder {
    struct Retrospection;
}

namespace placeholder::detail::sql_policy {

    enum struct StorageAtom {
        string,
        integer,
        reference,
    };

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

    inline void bind_string(sqlite3_stmt* statement, int index, const void* source) {
        const auto& value = *static_cast<const std::string*>(source);
        sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    inline void bind_integer(sqlite3_stmt* statement, int index, const void* source) {
        const auto& value = *static_cast<const std::int32_t*>(source);
        sqlite3_bind_int(statement, index, value);
    }

    template<typename Reference>
    void bind_reference(sqlite3_stmt* statement, int index, const void* source) {
        const auto& value = *static_cast<const Reference*>(source);
        sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value.raw()));
    }

    template<typename T, typename = void>
    struct atom {
        static constexpr bool supported = false;

        static consteval void require() {
            reject_if_forbidden<T>();
            static_assert(always_false<T>, "Missing SQL atom policy specialization for this type");
        }
    };

    template<>
    struct atom<std::string> {
        static constexpr bool supported = true;
        static constexpr auto storage = StorageAtom::string;
        static constexpr auto bind = &bind_string;

        static consteval void require() {}
    };

    template<>
    struct atom<std::int32_t> {
        static constexpr bool supported = true;
        static constexpr auto storage = StorageAtom::integer;
        static constexpr auto bind = &bind_integer;

        static consteval void require() {}
    };

    template<typename Meta, typename BaseType>
    struct atom<fqsm::Identifier<Meta, BaseType>> {
        static constexpr bool supported = true;
        static constexpr auto storage = StorageAtom::reference;
        static constexpr auto bind = &bind_reference<fqsm::Identifier<Meta, BaseType>>;

        static consteval void require() {}
    };

    template<typename T>
    struct nullable {
        static constexpr bool supported = false;

        static consteval void require() {
            reject_if_forbidden<T>();
            static_assert(always_false<T>, "Missing SQL nullable policy specialization for this type");
        }
    };

    template<typename T>
    struct nullable<std::optional<T>> {
        static constexpr bool supported = atom<T>::supported;
        using value_type = T;

        static consteval void require() {
            atom<T>::require();
        }
    };

    template<typename T>
    struct nullable<base::maybe<T>> {
        static constexpr bool supported = atom<T>::supported;
        using value_type = T;

        static consteval void require() {
            atom<T>::require();
        }
    };

    template<typename T, typename = void>
    struct sequence {
        static constexpr bool supported = false;

        static consteval void require() {
            reject_if_forbidden<T>();
            static_assert(always_false<T>, "Missing SQL sequence policy specialization for this type");
        }
    };

    template<typename Sequence>
    auto count_elements(const void* source) -> std::size_t {
        return static_cast<const Sequence*>(source)->size();
    }

    template<typename Sequence>
    auto element_at(const void* source, std::size_t ordinal) -> const void* {
        return std::addressof((*static_cast<const Sequence*>(source))[ordinal]);
    }

    template<typename T>
    struct sequence<std::vector<T>> {
        static constexpr bool supported = atom<T>::supported;
        static constexpr auto storage = atom<T>::storage;
        static constexpr auto bind = atom<T>::bind;
        static constexpr auto count = &count_elements<std::vector<T>>;
        static constexpr auto element = &element_at<std::vector<T>>;
        using value_type = T;

        static consteval void require() {
            atom<T>::require();
        }
    };

}
