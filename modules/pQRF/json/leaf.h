#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <fQSM/identifier.h>
#include <pQRF/json/document.h>

namespace fqsm::processing::persistency::json::detail::leaf {

    template<typename T>
    inline constexpr bool always_false = false;

    template<typename T>
    struct codec {
        static consteval void require() {
            static_assert(always_false<T>, "Missing JSON leaf codec for this type");
        }
    };

    template<>
    struct codec<std::string> {
        static auto write(const std::string& value) -> Value {
            return Value::string_value(value);
        }

        static void read(const Value& value, std::string& target) {
            if (value.kind != Value::Kind::string)
                throw std::runtime_error("json leaf: expected string");
            target = value.string;
        }

        static consteval void require() {}
    };

    template<>
    struct codec<std::int32_t> {
        static auto write(const std::int32_t& value) -> Value {
            return Value::number_value(value);
        }

        static void read(const Value& value, std::int32_t& target) {
            if (value.kind != Value::Kind::number)
                throw std::runtime_error("json leaf: expected number");
            target = static_cast<std::int32_t>(value.number);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<bool> {
        static auto write(const bool& value) -> Value {
            return Value::boolean_value(value);
        }

        static void read(const Value& value, bool& target) {
            if (value.kind != Value::Kind::boolean)
                throw std::runtime_error("json leaf: expected boolean");
            target = value.boolean;
        }

        static consteval void require() {}
    };

    inline auto format_hex_id(std::uint64_t raw) -> std::string {
        return std::format("0x{:x}", raw);
    }

    inline auto parse_hex_id(std::string_view text) -> std::uint64_t {
        if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            return std::stoull(std::string{text}, nullptr, 16);
        return std::stoull(std::string{text}, nullptr, 0);
    }

    template<typename Meta, typename BaseType>
    struct codec<fqsm::Identifier<Meta, BaseType>> {
        static auto write(const fqsm::Identifier<Meta, BaseType>& value) -> Value {
            return Value::string_value(format_hex_id(static_cast<std::uint64_t>(value.raw())));
        }

        static void read(const Value& value, fqsm::Identifier<Meta, BaseType>& target) {
            if (value.kind == Value::Kind::string) {
                target = fqsm::Identifier<Meta, BaseType>{
                    static_cast<BaseType>(parse_hex_id(value.string))
                };
                return;
            }
            if (value.kind == Value::Kind::number) {
                target = fqsm::Identifier<Meta, BaseType>{static_cast<BaseType>(value.number)};
                return;
            }
            throw std::runtime_error("json leaf: expected hex-string id");
        }

        static consteval void require() {}
    };

    template<typename T>
    auto write(const T& value) -> Value {
        codec<T>::require();
        return codec<T>::write(value);
    }

    template<typename T>
    void read(const Value& value, T& target) {
        codec<T>::require();
        codec<T>::read(value, target);
    }

}
