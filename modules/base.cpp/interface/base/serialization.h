#pragma once

#include <chrono>
#include <concepts>
#include <cctype>
#include <iomanip>
#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/pfr/core.hpp>
#include <boost/pfr/traits.hpp>

namespace base::serialization {

    template<typename T>
    using bare = std::remove_cvref_t<T>;

    template<typename T, typename Enable = void>
    struct codec;

    namespace detail {
        struct PfrFor {};

        template<typename T>
        concept reflectable_by_pfr = boost::pfr::is_implicitly_reflectable_v<bare<T>, PfrFor>;

        template<typename T>
        concept raw_value_roundtrip = requires {
            typename bare<T>::Raw;
        } and requires(const bare<T>& value, typename bare<T>::Raw raw) {
            { value.raw() } -> std::same_as<typename bare<T>::Raw>;
            bare<T>{raw};
        } and (not std::same_as<bare<T>, typename bare<T>::Raw>);

        template<typename T>
        concept stream_roundtrip = requires(std::ostream& out, std::istream& in, bare<T>& value, const bare<T>& constValue) {
            { out << constValue } -> std::same_as<std::ostream&>;
            { in >> value } -> std::same_as<std::istream&>;
        };

        [[noreturn]] inline void fail(std::string_view message) {
            throw std::runtime_error(std::string("serialization error: ") + std::string(message));
        }

        inline void skip_ws(std::istream& in) {
            for (;;) {
                const auto ch = in.peek();
                if (ch == std::char_traits<char>::eof()) return;
                if (!std::isspace(static_cast<unsigned char>(ch))) return;
                in.get();
            }
        }

        inline bool try_consume(std::istream& in, char token) {
            skip_ws(in);
            if (in.peek() != token) return false;
            in.get();
            return true;
        }

        inline void expect(std::istream& in, char token) {
            skip_ws(in);
            char got = '\0';
            if (!in.get(got) || got != token) {
                fail("unexpected token");
            }
        }

        inline void expect_word(std::istream& in, std::string_view word) {
            skip_ws(in);
            for (const char ch : word) {
                char got = '\0';
                if (!in.get(got) || got != ch) {
                    fail("unexpected word");
                }
            }
        }

        template<typename T>
        auto read(std::istream& in) -> bare<T>;

        template<typename T>
        void write(std::ostream& out, const T& value);

        template<typename Callback>
        void read_sequence(std::istream& in, char open, char close, Callback&& readOne) {
            expect(in, open);
            if (try_consume(in, close)) return;

            for (;;) {
                readOne();
                if (try_consume(in, close)) return;
                expect(in, ',');
            }
        }

        template<typename Range, typename Callback>
        void write_sequence(std::ostream& out, char open, char close, const Range& range, Callback&& writeOne) {
            out << open;
            bool first = true;
            for (const auto& entry : range) {
                if (!first) out << ", ";
                writeOne(entry);
                first = false;
            }
            out << close;
        }

        template<std::size_t I, typename T>
        void write_pfr_field(std::ostream& out, const T& value) {
            if constexpr (I != 0) out << ", ";
            detail::write(out, boost::pfr::get<I>(value));
        }

        template<typename T, std::size_t... I>
        void write_pfr_fields(std::ostream& out, const T& value, std::index_sequence<I...>) {
            (write_pfr_field<I>(out, value), ...);
        }

        template<std::size_t I, typename T>
        auto read_pfr_field(std::istream& in) -> bare<boost::pfr::tuple_element_t<I, T>> {
            if constexpr (I != 0) expect(in, ',');
            return detail::read<boost::pfr::tuple_element_t<I, T>>(in);
        }

        template<typename T, std::size_t... I>
        auto construct_from_tuple(std::tuple<bare<boost::pfr::tuple_element_t<I, T>>...>&& fields, std::index_sequence<I...>) -> T {
            return T{std::get<I>(std::move(fields))...};
        }

        template<typename T, std::size_t... I>
        auto read_pfr_fields(std::istream& in, std::index_sequence<I...>) -> T {
            expect(in, '{');
            auto fields = std::tuple<bare<boost::pfr::tuple_element_t<I, T>>...>{
                read_pfr_field<I, T>(in)...
            };
            expect(in, '}');
            return construct_from_tuple<T>(std::move(fields), std::index_sequence<I...>{});
        }
    }

    template<typename T>
    auto read(std::istream& in) -> bare<T> {
        return codec<bare<T>>::read(in);
    }

    template<typename T>
    void read(std::istream& in, T& value) requires std::assignable_from<T&, bare<T>> {
        value = read<bare<T>>(in);
    }

    template<typename T>
    void write(std::ostream& out, const T& value) {
        codec<bare<T>>::write(out, value);
    }

    template<typename T>
    auto from_string(std::string_view text) -> bare<T> {
        std::istringstream in{std::string(text)};
        auto value = read<bare<T>>(in);
        detail::skip_ws(in);
        if (in.peek() != std::char_traits<char>::eof()) {
            detail::fail("trailing data");
        }
        return value;
    }

    template<typename T>
    auto to_string(const T& value) -> std::string {
        std::ostringstream out;
        write(out, value);
        return out.str();
    }

    template<typename T>
    concept readable = requires(std::istream& in) {
        { base::serialization::read<bare<T>>(in) } -> std::same_as<bare<T>>;
    };

    template<typename T>
    concept writable = requires(std::ostream& out, const bare<T>& value) {
        { base::serialization::write(out, value) } -> std::same_as<void>;
    };

    template<typename T>
    concept serializable = readable<T> and writable<T>;

    namespace detail {
        template<typename T>
        auto read(std::istream& in) -> bare<T> {
            return base::serialization::read<bare<T>>(in);
        }

        template<typename T>
        void write(std::ostream& out, const T& value) {
            base::serialization::write(out, value);
        }
    }

    template<typename T>
    struct codec<T, std::enable_if_t<std::is_integral_v<T> && !std::same_as<T, bool>>> {
        static auto read(std::istream& in) -> T {
            detail::skip_ws(in);
            T value{};
            if (!(in >> value)) detail::fail("failed to read integer");
            return value;
        }

        static void write(std::ostream& out, T value) {
            out << value;
        }
    };

    template<typename T>
    struct codec<T, std::enable_if_t<std::is_floating_point_v<T>>> {
        static auto read(std::istream& in) -> T {
            detail::skip_ws(in);
            T value{};
            if (!(in >> value)) detail::fail("failed to read floating point");
            return value;
        }

        static void write(std::ostream& out, T value) {
            out << value;
        }
    };

    template<typename Clock, typename Duration>
    struct codec<std::chrono::time_point<Clock, Duration>, void> {
        using TimePoint = std::chrono::time_point<Clock, Duration>;
        using Rep = typename Duration::rep;

        static auto read(std::istream& in) -> TimePoint {
            return TimePoint{Duration{detail::read<Rep>(in)}};
        }

        static void write(std::ostream& out, const TimePoint& value) {
            detail::write(out, value.time_since_epoch().count());
        }
    };

    template<>
    struct codec<bool> {
        static auto read(std::istream& in) -> bool {
            detail::skip_ws(in);
            bool value = false;
            if (!(in >> std::boolalpha >> value)) detail::fail("failed to read bool");
            return value;
        }

        static void write(std::ostream& out, bool value) {
            out << std::boolalpha << value;
        }
    };

    template<typename T>
    struct codec<T, std::enable_if_t<std::is_enum_v<T>>> {
        static auto read(std::istream& in) -> T {
            using Raw = std::underlying_type_t<T>;
            return static_cast<T>(detail::read<Raw>(in));
        }

        static void write(std::ostream& out, T value) {
            detail::write(out, static_cast<std::underlying_type_t<T>>(value));
        }
    };

    template<>
    struct codec<std::string> {
        static auto read(std::istream& in) -> std::string {
            detail::skip_ws(in);
            std::string value;
            if (!(in >> std::quoted(value))) detail::fail("failed to read string");
            return value;
        }

        static void write(std::ostream& out, const std::string& value) {
            out << std::quoted(value);
        }
    };

    template<typename T>
    struct codec<T, std::enable_if_t<detail::raw_value_roundtrip<T>>> {
        static auto read(std::istream& in) -> T {
            return T{detail::read<typename T::Raw>(in)};
        }

        static void write(std::ostream& out, const T& value) {
            detail::write(out, value.raw());
        }
    };

    template<typename First, typename Second>
    struct codec<std::pair<First, Second>, void> {
        static auto read(std::istream& in) -> std::pair<First, Second> {
            detail::expect(in, '{');
            auto first = detail::read<First>(in);
            detail::expect(in, ',');
            auto second = detail::read<Second>(in);
            detail::expect(in, '}');
            return {std::move(first), std::move(second)};
        }

        static void write(std::ostream& out, const std::pair<First, Second>& value) {
            out << '{';
            detail::write(out, value.first);
            out << ", ";
            detail::write(out, value.second);
            out << '}';
        }
    };

    template<typename T>
    struct codec<std::optional<T>, void> {
        static auto read(std::istream& in) -> std::optional<T> {
            detail::skip_ws(in);
            if (in.peek() == 'n') {
                detail::expect_word(in, "null");
                return std::nullopt;
            }

            detail::expect(in, '(');
            auto value = detail::read<T>(in);
            detail::expect(in, ')');
            return std::optional<T>{std::move(value)};
        }

        static void write(std::ostream& out, const std::optional<T>& value) {
            if (!value.has_value()) {
                out << "null";
                return;
            }

            out << '(';
            detail::write(out, *value);
            out << ')';
        }
    };

    template<typename T>
    struct codec<std::vector<T>, void> {
        static auto read(std::istream& in) -> std::vector<T> {
            std::vector<T> out;
            detail::read_sequence(in, '[', ']', [&] {
                out.push_back(detail::read<T>(in));
            });
            return out;
        }

        static void write(std::ostream& out, const std::vector<T>& value) {
            detail::write_sequence(out, '[', ']', value, [&](const auto& entry) {
                detail::write(out, entry);
            });
        }
    };

    template<typename T>
    struct codec<std::set<T>, void> {
        static auto read(std::istream& in) -> std::set<T> {
            std::set<T> out;
            detail::read_sequence(in, '[', ']', [&] {
                out.insert(detail::read<T>(in));
            });
            return out;
        }

        static void write(std::ostream& out, const std::set<T>& value) {
            detail::write_sequence(out, '[', ']', value, [&](const auto& entry) {
                detail::write(out, entry);
            });
        }
    };

    template<typename T>
    struct codec<std::unordered_set<T>, void> {
        static auto read(std::istream& in) -> std::unordered_set<T> {
            std::unordered_set<T> out;
            detail::read_sequence(in, '[', ']', [&] {
                out.insert(detail::read<T>(in));
            });
            return out;
        }

        static void write(std::ostream& out, const std::unordered_set<T>& value) {
            detail::write_sequence(out, '[', ']', value, [&](const auto& entry) {
                detail::write(out, entry);
            });
        }
    };

    template<typename Key, typename Value>
    struct codec<std::map<Key, Value>, void> {
        static auto read(std::istream& in) -> std::map<Key, Value> {
            std::map<Key, Value> out;
            detail::read_sequence(in, '[', ']', [&] {
                const auto entry = detail::read<std::pair<Key, Value>>(in);
                out.emplace(entry.first, entry.second);
            });
            return out;
        }

        static void write(std::ostream& out, const std::map<Key, Value>& value) {
            detail::write_sequence(out, '[', ']', value, [&](const auto& entry) {
                detail::write(out, std::pair<Key, Value>{entry.first, entry.second});
            });
        }
    };

    template<typename Key, typename Value>
    struct codec<std::unordered_map<Key, Value>, void> {
        static auto read(std::istream& in) -> std::unordered_map<Key, Value> {
            std::unordered_map<Key, Value> out;
            detail::read_sequence(in, '[', ']', [&] {
                const auto entry = detail::read<std::pair<Key, Value>>(in);
                out.emplace(entry.first, entry.second);
            });
            return out;
        }

        static void write(std::ostream& out, const std::unordered_map<Key, Value>& value) {
            detail::write_sequence(out, '[', ']', value, [&](const auto& entry) {
                detail::write(out, std::pair<Key, Value>{entry.first, entry.second});
            });
        }
    };

    template<typename T>
    struct codec<T, std::enable_if_t<
        detail::stream_roundtrip<T>
        && !detail::reflectable_by_pfr<T>
        && !std::same_as<T, std::string>
        && !std::is_integral_v<T>
        && !std::is_floating_point_v<T>
        && !std::is_enum_v<T>
        && !detail::raw_value_roundtrip<T>
    >> {
        static auto read(std::istream& in) -> T {
            detail::skip_ws(in);
            T value{};
            if (!(in >> value)) detail::fail("failed to read stream value");
            return value;
        }

        static void write(std::ostream& out, const T& value) {
            out << value;
        }
    };

    template<typename T>
    struct codec<T, std::enable_if_t<
        detail::reflectable_by_pfr<T>
        && !std::is_integral_v<T>
        && !std::is_floating_point_v<T>
        && !std::is_enum_v<T>
        && !std::same_as<T, std::string>
        && !detail::raw_value_roundtrip<T>
    >> {
        static auto read(std::istream& in) -> T {
            return detail::read_pfr_fields<T>(in, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
        }

        static void write(std::ostream& out, const T& value) {
            out << '{';
            detail::write_pfr_fields(out, value, std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
            out << '}';
        }
    };

} // namespace base::serialization

namespace base {

    template<typename T>
    requires serialization::writable<T>
    [[nodiscard]] auto encoded(const T& value) -> std::string {
        return serialization::to_string(value);
    }

}
