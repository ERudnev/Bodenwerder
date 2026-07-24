#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <base/maybe.h>
#include <base/types/common_types.h>
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

        static auto decode(const Value& value) -> std::string {
            if (value.kind != Value::Kind::string)
                throw std::runtime_error("json leaf: expected string");
            return value.string;
        }

        static void read(const Value& value, std::string& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<std::int32_t> {
        static auto write(const std::int32_t& value) -> Value {
            return Value::number_value(value);
        }

        static auto decode(const Value& value) -> std::int32_t {
            if (value.kind == Value::Kind::number)
                return static_cast<std::int32_t>(value.number);
            if (value.kind == Value::Kind::real)
                return static_cast<std::int32_t>(value.real);
            throw std::runtime_error("json leaf: expected number");
        }

        static void read(const Value& value, std::int32_t& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<bool> {
        static auto write(const bool& value) -> Value {
            return Value::boolean_value(value);
        }

        static auto decode(const Value& value) -> bool {
            if (value.kind != Value::Kind::boolean)
                throw std::runtime_error("json leaf: expected boolean");
            return value.boolean;
        }

        static void read(const Value& value, bool& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<float> {
        static auto write(const float& value) -> Value {
            return Value::real_value(static_cast<double>(value));
        }

        static auto decode(const Value& value) -> float {
            if (value.kind == Value::Kind::real)
                return static_cast<float>(value.real);
            if (value.kind == Value::Kind::number)
                return static_cast<float>(value.number);
            throw std::runtime_error("json leaf: expected real");
        }

        static void read(const Value& value, float& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<double> {
        static auto write(const double& value) -> Value {
            return Value::real_value(value);
        }

        static auto decode(const Value& value) -> double {
            if (value.kind == Value::Kind::real)
                return value.real;
            if (value.kind == Value::Kind::number)
                return static_cast<double>(value.number);
            throw std::runtime_error("json leaf: expected real");
        }

        static void read(const Value& value, double& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<std::filesystem::path> {
        static auto write(const std::filesystem::path& value) -> Value {
            return Value::string_value(value.generic_string());
        }

        static auto decode(const Value& value) -> std::filesystem::path {
            if (value.kind != Value::Kind::string)
                throw std::runtime_error("json leaf: expected string path");
            return std::filesystem::path{value.string};
        }

        static void read(const Value& value, std::filesystem::path& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<std::chrono::system_clock::time_point> {
        using Timepoint = std::chrono::system_clock::time_point;

        static auto write(const Timepoint& value) -> Value {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();
            return Value::number_value(static_cast<std::int64_t>(ms));
        }

        static auto decode(const Value& value) -> Timepoint {
            if (value.kind != Value::Kind::number)
                throw std::runtime_error("json leaf: expected integer milliseconds timepoint");
            return Timepoint{std::chrono::milliseconds{value.number}};
        }

        static void read(const Value& value, Timepoint& target) {
            target = decode(value);
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
        using Id = fqsm::Identifier<Meta, BaseType>;

        static auto write(const Id& value) -> Value {
            return Value::string_value(format_hex_id(static_cast<std::uint64_t>(value.raw())));
        }

        static auto decode(const Value& value) -> Id {
            if (value.kind == Value::Kind::string)
                return Id{static_cast<BaseType>(parse_hex_id(value.string))};
            if (value.kind == Value::Kind::number)
                return Id{static_cast<BaseType>(value.number)};
            throw std::runtime_error("json leaf: expected hex-string id");
        }

        static void read(const Value& value, Id& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<typename T>
    struct codec<std::optional<T>> {
        static auto write(const std::optional<T>& value) -> Value {
            if (!value.has_value())
                return Value::null();
            return codec<T>::write(*value);
        }

        static auto decode(const Value& value) -> std::optional<T> {
            if (value.kind == Value::Kind::null)
                return std::nullopt;
            return codec<T>::decode(value);
        }

        static void read(const Value& value, std::optional<T>& target) {
            target = decode(value);
        }

        static consteval void require() {
            codec<T>::require();
        }
    };

    template<typename T>
    struct codec<base::maybe<T>> {
        static auto write(const base::maybe<T>& value) -> Value {
            if (!value.exists())
                return Value::null();
            return codec<T>::write(*value);
        }

        static auto decode(const Value& value) -> base::maybe<T> {
            if (value.kind == Value::Kind::null)
                return std::nullopt;
            return codec<T>::decode(value);
        }

        static void read(const Value& value, base::maybe<T>& target) {
            target = decode(value);
        }

        static consteval void require() {
            codec<T>::require();
        }
    };

    template<>
    struct codec<base::common_types::index2> {
        using index2 = base::common_types::index2;

        static auto write(const index2& value) -> Value {
            return Value::array_value({
                codec<base::common_types::integer>::write(value.x),
                codec<base::common_types::integer>::write(value.y),
            });
        }

        static auto decode(const Value& value) -> index2 {
            if (!value.is_array() || value.array.size() != 2)
                throw std::runtime_error("json leaf: expected index2 array");
            return index2{
                .x = codec<base::common_types::integer>::decode(value.array[0]),
                .y = codec<base::common_types::integer>::decode(value.array[1]),
            };
        }

        static void read(const Value& value, index2& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<glm::vec2> {
        static auto write(const glm::vec2& value) -> Value {
            return Value::array_value({
                codec<float>::write(value.x),
                codec<float>::write(value.y),
            });
        }

        static auto decode(const Value& value) -> glm::vec2 {
            if (!value.is_array() || value.array.size() != 2)
                throw std::runtime_error("json leaf: expected vec2 array");
            return glm::vec2{
                codec<float>::decode(value.array[0]),
                codec<float>::decode(value.array[1]),
            };
        }

        static void read(const Value& value, glm::vec2& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    template<>
    struct codec<glm::vec3> {
        static auto write(const glm::vec3& value) -> Value {
            return Value::array_value({
                codec<float>::write(value.x),
                codec<float>::write(value.y),
                codec<float>::write(value.z),
            });
        }

        static auto decode(const Value& value) -> glm::vec3 {
            if (!value.is_array() || value.array.size() != 3)
                throw std::runtime_error("json leaf: expected vec3 array");
            return glm::vec3{
                codec<float>::decode(value.array[0]),
                codec<float>::decode(value.array[1]),
                codec<float>::decode(value.array[2]),
            };
        }

        static void read(const Value& value, glm::vec3& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    // Enum as integer discriminant (same row / same JSON number — not a string name).
    template<typename T>
        requires std::is_enum_v<T>
    struct codec<T> {
        static auto write(const T& value) -> Value {
            return codec<std::int32_t>::write(static_cast<std::int32_t>(value));
        }

        static auto decode(const Value& value) -> T {
            return static_cast<T>(codec<std::int32_t>::decode(value));
        }

        static void read(const Value& value, T& target) {
            target = decode(value);
        }

        static consteval void require() {}
    };

    // Containers (vector / umap / …) are not leaves: use describe collection<> or nested field<>.

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

    template<typename T>
    auto decode(const Value& value) -> T {
        codec<T>::require();
        return codec<T>::decode(value);
    }

}
