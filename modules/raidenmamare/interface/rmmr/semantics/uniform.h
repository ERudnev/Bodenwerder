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
            samplerBuffer,
        };

        struct Entry {
            PersistentId id;
            Type type;
            Name name;
        };

        // Persistent uniform semantics vocabulary.
        //
        // ID convention:
        // - 1..99: matrices and structural transforms
        // - 100..1999: world / material / light pixel channels
        // - 2000..: screen-space overlay (and kin)
        static constexpr auto vocabulary = std::array<Entry, 29>{{
            Entry{0, Type::i32, "_undefined"},

            // triangle.vert.glsl
            Entry{1, Type::m4f, "model"},
            Entry{2, Type::m4f, "view"},
            Entry{3, Type::m4f, "projection"},
            Entry{4, Type::m4f, "lightSpaceMatrix"},

            // triangle.frag.glsl (pixel channels start at 100)
            Entry{100, Type::v3f, "albedo"},
            Entry{101, Type::v3f, "ambientColor"},
            Entry{102, Type::f32, "ambientIntensity"},

            // first real lamp (OpenGL culture: light0)
            Entry{103, Type::v3f, "light0Color"},
            Entry{104, Type::f32, "light0Intensity"},
            Entry{105, Type::v3f, "light0Pos"},

            // Grid.frag.glsl (shader-based ground grid; GLSL: u_patternScale, u_colorPrimary, u_colorSecondary)
            Entry{106, Type::f32, "patternScale"},
            Entry{107, Type::v3f, "colorPrimary"},
            Entry{108, Type::v3f, "colorSecondary"},

            Entry{109, Type::sampler2d, "shadowMap"},
            Entry{110, Type::sampler2d, "albedoMap"},
            Entry{111, Type::sampler2d, "atlasTexture"},
            Entry{112, Type::samplerBuffer, "atlasEntries"},
            Entry{113, Type::i32, "spriteIndex"},
            Entry{114, Type::v2f, "inverseAtlasSize"},
            Entry{115, Type::f32, "opacity"},
            Entry{116, Type::i32, "scenicAlias"},

            // Overlay / screen-space (gap after world materials — start at 2000)
            Entry{2000, Type::sampler2d, "sceneColor"},
            Entry{2001, Type::sampler2d, "identiffyMap"},
            Entry{2002, Type::v2f, "texelSize"},
            Entry{2003, Type::i32, "under"},
            Entry{2004, Type::i32, "selectedCount"},
            Entry{2005, Type::i32, "selected"}, // uniform array base; upload via glUniform1uiv
            Entry{2006, Type::sampler2d, "selectedMap"},
        }};

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

        using RuntimeMapping = umap<PersistentId, RenderId>;
    };
}