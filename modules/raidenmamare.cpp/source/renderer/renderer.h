#pragma once

#include <Raidenmamare/resources/material.q1.h>
#include <Raidenmamare/scene/actor.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/node.q1.h>
#include <Raidenmamare/scene/root.q1.h>
#include <Raidenmamare/system/viewport.q1.h>
#include <Raidenmamare/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    class Renderer final {
    public:
        struct PassArguments {
            fqsm::Reading world;
            system::Viewport::Id viewport;
            system::Window::Id window;
            scene::Root::Id scene;
            scene::Camera::Id camera;
        };

        void render(PassArguments args);

    private:
        void bind_material(PassArguments args, resource::Material::Id material);
        void bind_actor(PassArguments args, resource::Material::Id material, const scene::PrimitiveActor::Quantum& actor, scene::Node::Id node);
        void bind_lights(PassArguments args, resource::Material::Id material);
    };

}
