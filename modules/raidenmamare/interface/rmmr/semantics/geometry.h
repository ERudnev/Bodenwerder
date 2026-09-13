#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fQSM/api/builtins.h>

namespace rmmr {
    // use q1 type aliases as own.. ever
    using namespace fqsm::q1;
}

namespace rmmr::primitive {

    struct GeometrySemantics {
        using Name = std::string_view;
        using PersistentId = integer;

        enum class Type : std::uint8_t {
            f32,
            v2f,
            v3f,
            v4f,
            u32,
            uvec2,
        };

        struct Entry {
            PersistentId id;
            Type type;
            Name name;
            bool live;
        };

        // Persistent geometry channel semantics vocabulary.
        //
        // ID convention:
        // - 1..99: primary vertex attributes
        // - 100..: auxiliary / optional attributes
        // `live` — own VBO (Runtime::channels), not interleaved.
        static constexpr auto vocabulary = std::array<Entry, 9>{{
            Entry{0, Type::f32, "_undefined", false},

            Entry{1, Type::v3f, "position", false},
            Entry{2, Type::v3f, "normal", false},
            Entry{3, Type::v2f, "uv0", false},
            Entry{100, Type::v4f, "color0", false},
            Entry{101, Type::uvec2, "mix0", false},
            Entry{102, Type::f32, "cohesion", true},
            Entry{103, Type::u32, "palette", false},
            Entry{104, Type::uvec2, "weights", false},
        }};

        static constexpr auto name_of(PersistentId id) -> Name {
            if (const auto* e = find(id)) return e->name;
            throw std::runtime_error("GeometrySemantics::name_of: unknown geometry semantic id");
        }

        static auto id_of(Name name) -> PersistentId {
            for (const auto& e : vocabulary) {
                if (e.name == name) return e.id;
            }
            throw std::runtime_error("GeometrySemantics::id_of: unknown geometry semantic name");
        }

        static auto layoutIds(const vector<string>& names) -> vector<PersistentId> {
            vector<PersistentId> out;
            out.reserve(names.size());
            for (const auto& name : names) {
                const auto id = id_of(name);
                if (id == PersistentId{0}) {
                    throw std::runtime_error("GeometrySemantics::layoutIds: unknown geometry channel semantic: " + name);
                }
                out.push_back(id);
            }
            return out;
        }

        static constexpr auto find(PersistentId id) -> const Entry* {
            for (const auto& e : vocabulary) {
                if (e.id == id) return &e;
            }
            return nullptr;
        }

        static constexpr auto type_of(PersistentId id) -> Type {
            if (const auto* e = find(id)) return e->type;
            throw std::runtime_error("GeometrySemantics::type_of: unknown geometry semantic id");
        }

        static constexpr auto byteSize(Type type) -> std::size_t {
            switch (type) {
                case Type::f32: return 4;
                case Type::v2f: return 8;
                case Type::v3f: return 12;
                case Type::v4f: return 16;
                case Type::u32: return 4;
                case Type::uvec2: return 8;
            }
            throw std::runtime_error("GeometrySemantics::byteSize: unknown geometry type");
        }

        static constexpr auto componentCount(Type type) -> integer {
            switch (type) {
                case Type::f32: return 1;
                case Type::v2f: return 2;
                case Type::v3f: return 3;
                case Type::v4f: return 4;
                case Type::u32: return 1;
                case Type::uvec2: return 2;
            }
            throw std::runtime_error("GeometrySemantics::componentCount: unknown geometry type");
        }

        static constexpr auto integerPacked(Type type) -> bool {
            return type == Type::u32 or type == Type::uvec2;
        }
    };
}
