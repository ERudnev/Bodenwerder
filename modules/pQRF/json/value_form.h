#pragma once

// Value form: leaf atom, pair entry, or nested Retrospection<T> product.
// Used by JSON row walkers; DB uses form_tree.h for relational nested tables.

#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <fQSM/aspect/persistency.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/retrospection.h>
#include <fQSM/utility/bad_value.h>
#include <pQRF/json/document.h>
#include <pQRF/json/leaf.h>

namespace fqsm::processing::persistency::json::detail::form {

    using fqsm::aspect::Collection;
    using fqsm::aspect::Field;

    template<typename T>
    concept HasRetrospection = requires(fqsm::detail::meta::category::RetrospectionProbe& d) {
        fqsm::aspect::Retrospection<T>::describe(d);
    };

    template<typename T>
    struct is_pair : std::false_type {};
    template<typename A, typename B>
    struct is_pair<std::pair<A, B>> : std::true_type {};

    template<typename T>
    auto write_value(const T& value) -> Value;

    template<typename T>
    auto decode_value(const Value& value) -> T;

    template<typename T>
    void read_value(const Value& value, T& target);

    template<typename Root>
    struct WriteFormDesc {
        Value& row;
        const Root& root;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            row.array.push_back(write_value(slot.get(root)));
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            Value nested = Value::array_value();
            for (const auto& element : slot.get(root))
                nested.array.push_back(write_value(element));
            row.array.push_back(std::move(nested));
        }

        void all(auto&&) {}
    };

    template<typename Root>
    struct ReadFormDesc {
        const Value& row;
        Root& root;
        std::size_t index = 0;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            if (index >= row.array.size())
                throw std::runtime_error("json form: missing field");
            read_value(row.array[index++], slot.get(root));
        }

        template<typename Elem, auto... Members>
        void one(Collection<Elem, Members...> slot) {
            if (index >= row.array.size())
                throw std::runtime_error("json form: missing collection");
            const auto& nested = row.array[index++];
            if (!nested.is_array())
                throw std::runtime_error("json form: collection must be array");
            auto& container = slot.get(root);
            container.clear();
            for (const auto& element : nested.array) {
                if constexpr (requires { container.push_back(std::declval<Elem>()); })
                    container.push_back(decode_value<Elem>(element));
                else
                    container.insert(decode_value<Elem>(element));
            }
        }

        void all(auto&&) {}
    };

    template<typename T>
    auto write_retrospection(const T& value) -> Value {
        Value row = Value::array_value();
        WriteFormDesc<T> writer{row, value};
        fqsm::aspect::Retrospection<T>::describe(writer);
        return row;
    }

    template<typename T>
    auto decode_retrospection(const Value& value) -> T {
        if (!value.is_array())
            throw std::runtime_error("json form: expected retrospection array");
        T target = fqsm::utility::BadValue{};
        ReadFormDesc<T> reader{value, target, 0};
        fqsm::aspect::Retrospection<T>::describe(reader);
        return target;
    }

    template<typename A, typename B>
    auto write_pair(const std::pair<A, B>& value) -> Value {
        return Value::array_value({
            write_value(value.first),
            write_value(value.second),
        });
    }

    template<typename A, typename B>
    auto decode_pair(const Value& value) -> std::pair<A, B> {
        if (!value.is_array() || value.array.size() != 2)
            throw std::runtime_error("json form: expected pair array");
        return std::pair<A, B>{
            decode_value<std::remove_cv_t<A>>(value.array[0]),
            decode_value<B>(value.array[1]),
        };
    }

    template<typename T>
    auto write_value(const T& value) -> Value {
        if constexpr (is_pair<std::remove_cvref_t<T>>::value) {
            return write_pair(value);
        } else if constexpr (HasRetrospection<std::remove_cvref_t<T>>) {
            return write_retrospection(value);
        } else {
            return leaf::write(value);
        }
    }

    template<typename T>
    auto decode_value(const Value& value) -> T {
        using Bare = std::remove_cvref_t<T>;
        if constexpr (is_pair<Bare>::value) {
            return decode_pair<typename Bare::first_type, typename Bare::second_type>(value);
        } else if constexpr (HasRetrospection<Bare>) {
            return decode_retrospection<Bare>(value);
        } else {
            return leaf::decode<Bare>(value);
        }
    }

    template<typename T>
    void read_value(const Value& value, T& target) {
        target = decode_value<T>(value);
    }

}
