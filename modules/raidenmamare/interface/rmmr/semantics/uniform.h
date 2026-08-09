#pragma once

// just for int aliases from two worlds...
#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

#include <fQSM/api/builtins.h>

namespace rmmr {
    // use q1 type aliases as own.. ever
    using namespace fqsm::q1;
}

namespace rmmr::material {

    struct Semantics {
        using Name = std::string_view;
        using RenderId = GLint;
        using PersistentId = integer;

        static auto uniform_name(Name name) -> std::string {
            return std::string("u_").append(name);
        }

        enum class Type : std::uint8_t {
            f32,
            i32,
            v2f,
            v3f,
            m4f,
            sampler2d,
            sampler2dArray,
            ssbo,
        };

        // Texture unit / SSBO binding point. -1 = not a bound resource.
        using BindingPoint = GLint;

        struct Entry {
            PersistentId id;
            Type type;
            Name name;
            BindingPoint binding;
        };

        // Static resource and screen-space uniform vocabulary. Frame, actor, draw and
        // surface values live in the fixed UBO/SSBO contract, not this palette.
        // Texture units (contexts mutually exclusive per draw):
        // - 0: albedoMap / atlasTexture / sceneColor / compose overlay
        // - 1: shadowMap / identiffyMap
        // - 2: selectedMap
        // SSBO binding points:
        // - 0: atlasEntries
        static constexpr auto vocabulary = std::array<Entry, 13>{{
            Entry{0, Type::i32, "_undefined", -1},

            Entry{109, Type::sampler2d, "shadowMap", 1},
            Entry{110, Type::sampler2dArray, "albedoMap", 0},
            Entry{111, Type::sampler2d, "atlasTexture", 0},
            Entry{112, Type::ssbo, "atlasEntries", 0},
            Entry{114, Type::v2f, "inverseAtlasSize", -1},

            // Overlay / screen-space (gap after world materials — start at 2000)
            Entry{2000, Type::sampler2d, "sceneColor", 0},
            Entry{2001, Type::sampler2d, "identiffyMap", 1},
            Entry{2002, Type::v2f, "texelSize", -1},
            Entry{2003, Type::i32, "under", -1},
            Entry{2004, Type::i32, "selectedCount", -1},
            Entry{2005, Type::i32, "selected", -1}, // uniform array base; upload via glUniform1uiv
            Entry{2006, Type::sampler2d, "selectedMap", 2},
        }};

        static constexpr auto isBoundResource(Type type) -> bool {
            return type == Type::sampler2d or type == Type::sampler2dArray or type == Type::ssbo;
        }

        static constexpr auto name_of(PersistentId id) -> Name {
            for (const auto& e : vocabulary) {
                if (e.id == id) return e.name;
            }
            throw std::runtime_error("Semantics::name_of: unknown uniform semantic id");
        }

        static auto id_of(Name name) -> PersistentId {
            for (const auto& e : vocabulary) {
                if (e.name == name) return e.id;
            }
            throw std::runtime_error("Semantics::id_of: unknown uniform semantic name");
        }

        static auto ids_of(const vector<string>& names) -> vector<PersistentId> {
            vector<PersistentId> out;
            out.reserve(names.size());
            for (const auto& name : names) {
                const auto id = id_of(name);
                if (id == PersistentId{0}) {
                    throw std::runtime_error("Semantics::ids_of: unknown uniform semantic: " + name);
                }
                out.push_back(id);
            }
            return out;
        }

        static constexpr auto type_of(PersistentId id) -> Type {
            for (const auto& e : vocabulary) {
                if (e.id == id) return e.type;
            }
            throw std::runtime_error("Semantics::type_of: unknown uniform semantic id");
        }

        static constexpr auto binding_of(PersistentId id) -> BindingPoint {
            for (const auto& e : vocabulary) {
                if (e.id == id) return e.binding;
            }
            throw std::runtime_error("Semantics::binding_of: unknown uniform semantic id");
        }

        using RuntimeMapping = umap<PersistentId, RenderId>;
    };
}
