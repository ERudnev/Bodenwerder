#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/core.q1.h>

#include <cstdint>

#include <fQSM/api/interface.h>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {
            vector<Pos> slots;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::VertexArray vao;
            renderer::VertexBuffer vbo;
            renderer::ElementBuffer ebo;
            renderer::Count vertex_count;
            renderer::Count index_count;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Generator : Feature<Generator, Asset> {
        enum class Type : std::uint8_t {
            triangle,
            kube,
            gridPlane,
        };
        struct Quantum {
            Type type;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
