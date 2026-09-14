#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <glm/ext/vector_int4.hpp>

#include <array>
#include <cstdint>
#include <span>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    struct PatchGrid : Feature<PatchGrid, Node> {
        struct Patch {
            integer diamond;
            integer originU;
            integer originV;
            integer step;
        };
        struct Shell {
            std::array<vec4, 12> vertices;
            std::array<glm::ivec4, 10> diamonds;
        };
        struct FieldState {
            mat4 model;
            vec4 albedoOpacity;
            float radius;
            float amplitude;
            std::int32_t span;
            std::int32_t cells;
            vec4 shell[12];
            glm::ivec4 diamonds[10];
        };
        struct Quantum {
            system::Device::Id device;
            renderer::StorageBuffer actorState;
            renderer::StorageBuffer patches;
            renderer::StorageBuffer dummy;
            integer patchCount;
            float radius;
            float amplitude;
            integer span;
            integer cells;
            Shell shell;
            resource::geometry::Runtime::Id geometry;
            resource::material::Runtime::Id material;
            base::maybe<resource::texpack::Runtime::Id> texpack;
            resource::texture::Runtime::Id heightField;
            resource::texture::Runtime::Id coverField;
            renderer::IndirectBuffer indirect;
            renderer::Count drawCount;
        };
        struct Actions : BaseActions {
            static auto compose(Reading, resource::geometry::Asset::Id, resource::material::Asset::Id, resource::texpack::Pack::Id, resource::texture::Asset::Id, resource::texture::Asset::Id, const Shell&, std::span<const Patch>, float radius, float amplitude, integer span, integer cells) -> optional<Quantum>;
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
