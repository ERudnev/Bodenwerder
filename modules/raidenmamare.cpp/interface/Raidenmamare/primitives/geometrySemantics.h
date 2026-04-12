#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include <iQSM/api/builtins.h>

namespace rmmr {
    // use q1 type aliases as own.. ever
    using namespace iqsm::q1;
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
        };

        struct Entry {
            PersistentId id;
            Type type;
            Name name;
        };

        // Persistent geometry channel semantics vocabulary.
        //
        // ID convention:
        // - 1..99: primary vertex attributes
        // - 100..: auxiliary / optional attributes
        static constexpr auto vocabulary = std::array<Entry, 5>{{
            Entry{0, Type::f32, "_undefined"},

            Entry{1, Type::v3f, "position"},
            Entry{2, Type::v3f, "normal"},
            Entry{3, Type::v2f, "uv0"},
            Entry{100, Type::v4f, "color0"},
        }};

        static constexpr auto name_of(PersistentId id) -> Name {
            for (const auto& e : vocabulary) {
                if (e.id == id) return e.name;
            }
            throw std::runtime_error("GeometrySemantics::name_of: unknown geometry semantic id");
        }

        static auto id_of(Name name) -> PersistentId {
            for (const auto& e : vocabulary) {
                if (e.name == name) return e.id;
            }
            throw std::runtime_error("GeometrySemantics::id_of: unknown geometry semantic name");
        }

        static constexpr auto type_of(PersistentId id) -> Type {
            for (const auto& e : vocabulary) {
                if (e.id == id) return e.type;
            }
            throw std::runtime_error("GeometrySemantics::type_of: unknown geometry semantic id");
        }
    };

}

