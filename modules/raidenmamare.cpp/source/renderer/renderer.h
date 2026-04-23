#pragma once

#include <Raidenmamare/materials/core.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/scene/core.q1.h>
#include <Raidenmamare/scene/actor.q1.h>
#include <Raidenmamare/viewport.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr {

    using namespace iqsm::q1_gateway;

    class Renderer final {
    public:
        struct PassArguments {
            Reading world;
            Viewport::Id viewport;
            scene::Core::Id scene;
        };
        Renderer() = default;
        void render_new_temp(PassArguments);
    private:
        void bind_material(PassArguments, material::Core::RuntimeAccess);
        void bind_actor(PassArguments, material::Core::RuntimeAccess, const scene::PrimitiveActor::Quantum&, scene::Node::Id);
        void bind_lights(PassArguments, material::Core::RuntimeAccess);
    };
}

