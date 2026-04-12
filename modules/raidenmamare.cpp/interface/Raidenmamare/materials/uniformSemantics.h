#pragma once

// just for int aliases from two worlds...
#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

#include <iQSM/api/builtins.h>

namespace rmmr {
    // use q1 type aliases as own.. ever
    using namespace iqsm::q1;
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
            v3f,
            m4f,
        };

        struct Entry {
            PersistentId id;
            Type type;
            Name name;
        };

        struct Binding {
            PersistentId id;
            Type type;
            RenderId location;
        };
        
        // Persistent uniform semantics vocabulary.
        //
        // ID convention:
        // - 1..99: matrices and structural transforms
        // - 100..: "pixel channels" (material/light parameters, scalars/vectors, etc.)
        static constexpr auto vocabulary = std::array<Entry, 10>{{
            Entry{0, Type::i32, "_undefined"},

            // triangle.vert.glsl
            Entry{1, Type::m4f, "model"},
            Entry{2, Type::m4f, "view"},
            Entry{3, Type::m4f, "projection"},

            // triangle.frag.glsl (pixel channels start at 100)
            Entry{100, Type::v3f, "albedo"},
            Entry{101, Type::v3f, "ambientColor"},
            Entry{102, Type::f32, "ambientIntensity"},

            // first real lamp (OpenGL culture: light0)
            Entry{103, Type::v3f, "light0Color"},
            Entry{104, Type::f32, "light0Intensity"},
            Entry{105, Type::v3f, "light0Pos"},
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

        static constexpr auto type_of(PersistentId id) -> Type {
            for (const auto& e : vocabulary) {
                if (e.id == id) return e.type;
            }
            throw std::runtime_error("Semantics::type_of: unknown uniform semantic id");
        }      

        using RuntimeMapping = umap<PersistentId, RenderId>;
    };
}