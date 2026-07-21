#pragma once

// Minimal JSON DOM for pQRF positional archives (objects/arrays/scalars).

#include <cctype>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fqsm::processing::persistency::json {

    struct Value {
        enum struct Kind { null, boolean, number, string, array, object };

        Kind kind = Kind::null;
        bool boolean = false;
        std::int64_t number = 0;
        std::string string{};
        std::vector<Value> array{};
        std::vector<std::pair<std::string, Value>> object{};

        static auto null() -> Value { return {}; }

        static auto boolean_value(bool value) -> Value {
            Value out;
            out.kind = Kind::boolean;
            out.boolean = value;
            return out;
        }

        static auto number_value(std::int64_t value) -> Value {
            Value out;
            out.kind = Kind::number;
            out.number = value;
            return out;
        }

        static auto string_value(std::string value) -> Value {
            Value out;
            out.kind = Kind::string;
            out.string = std::move(value);
            return out;
        }

        static auto array_value(std::vector<Value> value = {}) -> Value {
            Value out;
            out.kind = Kind::array;
            out.array = std::move(value);
            return out;
        }

        static auto object_value() -> Value {
            Value out;
            out.kind = Kind::object;
            return out;
        }

        auto is_null() const -> bool { return kind == Kind::null; }
        auto is_array() const -> bool { return kind == Kind::array; }
        auto is_object() const -> bool { return kind == Kind::object; }

        auto find(std::string_view key) -> Value* {
            if (kind != Kind::object) return nullptr;
            for (auto& [name, value] : object) {
                if (name == key) return &value;
            }
            return nullptr;
        }

        auto find(std::string_view key) const -> const Value* {
            if (kind != Kind::object) return nullptr;
            for (const auto& [name, value] : object) {
                if (name == key) return &value;
            }
            return nullptr;
        }

        void set(std::string key, Value value) {
            if (kind != Kind::object) kind = Kind::object;
            if (auto* existing = find(key)) {
                *existing = std::move(value);
                return;
            }
            object.emplace_back(std::move(key), std::move(value));
        }
    };

    namespace detail::document {

        struct Parser {
            std::string_view text;
            std::size_t cursor = 0;

            [[noreturn]] void fail(std::string_view what) const {
                throw std::runtime_error(std::format("json parse at {}: {}", cursor, what));
            }

            void skip_ws() {
                while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])))
                    ++cursor;
            }

            auto peek() -> char {
                skip_ws();
                if (cursor >= text.size()) fail("unexpected end");
                return text[cursor];
            }

            auto take() -> char {
                const auto character = peek();
                ++cursor;
                return character;
            }

            void expect(char character) {
                if (take() != character) fail(std::format("expected '{}'", character));
            }

            auto parse_string() -> std::string {
                expect('"');
                std::string out;
                while (cursor < text.size()) {
                    const auto character = text[cursor++];
                    if (character == '"') return out;
                    if (character != '\\') {
                        out.push_back(character);
                        continue;
                    }
                    if (cursor >= text.size()) fail("broken escape");
                    const auto escaped = text[cursor++];
                    switch (escaped) {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        case 'u': fail("\\u escapes not supported");
                        default: fail("unknown escape");
                    }
                }
                fail("unterminated string");
            }

            auto parse_number() -> std::int64_t {
                skip_ws();
                const auto begin = cursor;
                if (cursor < text.size() && (text[cursor] == '-' || text[cursor] == '+')) ++cursor;
                if (cursor >= text.size() || !std::isdigit(static_cast<unsigned char>(text[cursor])))
                    fail("expected digit");
                while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])))
                    ++cursor;
                if (cursor < text.size() && (text[cursor] == '.' || text[cursor] == 'e' || text[cursor] == 'E'))
                    fail("only integer numbers are supported");
                return std::stoll(std::string{text.substr(begin, cursor - begin)});
            }

            auto parse_value() -> Value {
                const auto character = peek();
                if (character == 'n') {
                    expect('n'); expect('u'); expect('l'); expect('l');
                    return Value::null();
                }
                if (character == 't') {
                    expect('t'); expect('r'); expect('u'); expect('e');
                    return Value::boolean_value(true);
                }
                if (character == 'f') {
                    expect('f'); expect('a'); expect('l'); expect('s'); expect('e');
                    return Value::boolean_value(false);
                }
                if (character == '"') return Value::string_value(parse_string());
                if (character == '[') {
                    expect('[');
                    Value out = Value::array_value();
                    skip_ws();
                    if (peek() == ']') {
                        take();
                        return out;
                    }
                    while (true) {
                        out.array.push_back(parse_value());
                        skip_ws();
                        const auto separator = take();
                        if (separator == ']') break;
                        if (separator != ',') fail("expected ',' or ']'");
                    }
                    return out;
                }
                if (character == '{') {
                    expect('{');
                    Value out = Value::object_value();
                    skip_ws();
                    if (peek() == '}') {
                        take();
                        return out;
                    }
                    while (true) {
                        const auto key = parse_string();
                        skip_ws();
                        expect(':');
                        out.object.emplace_back(key, parse_value());
                        skip_ws();
                        const auto separator = take();
                        if (separator == '}') break;
                        if (separator != ',') fail("expected ',' or '}'");
                    }
                    return out;
                }
                if (character == '-' || std::isdigit(static_cast<unsigned char>(character)))
                    return Value::number_value(parse_number());
                fail("unexpected token");
            }
        };

        inline void write_string(std::string& out, std::string_view text) {
            out.push_back('"');
            for (const auto character : text) {
                switch (character) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out.push_back(character); break;
                }
            }
            out.push_back('"');
        }

        inline void write_value(std::string& out, const Value& value) {
            switch (value.kind) {
                case Value::Kind::null:
                    out += "null";
                    break;
                case Value::Kind::boolean:
                    out += value.boolean ? "true" : "false";
                    break;
                case Value::Kind::number:
                    out += std::format("{}", value.number);
                    break;
                case Value::Kind::string:
                    write_string(out, value.string);
                    break;
                case Value::Kind::array: {
                    out.push_back('[');
                    for (std::size_t index = 0; index < value.array.size(); ++index) {
                        if (index != 0) out += ", ";
                        write_value(out, value.array[index]);
                    }
                    out.push_back(']');
                    break;
                }
                case Value::Kind::object: {
                    out.push_back('{');
                    for (std::size_t index = 0; index < value.object.size(); ++index) {
                        if (index != 0) out += ", ";
                        write_string(out, value.object[index].first);
                        out += ": ";
                        write_value(out, value.object[index].second);
                    }
                    out.push_back('}');
                    break;
                }
            }
        }

    }

    inline auto parse(std::string_view text) -> Value {
        detail::document::Parser parser{text, 0};
        auto value = parser.parse_value();
        parser.skip_ws();
        if (parser.cursor != parser.text.size())
            parser.fail("trailing input");
        return value;
    }

    inline auto stringify(const Value& value) -> std::string {
        std::string out;
        detail::document::write_value(out, value);
        out.push_back('\n');
        return out;
    }

}
