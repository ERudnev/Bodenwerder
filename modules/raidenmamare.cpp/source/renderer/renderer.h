#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/material.q1.h>
#include <rmmr/resources/shadowMap.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    class Renderer final {
    public:
        struct FrameContext {
            fqsm::Reading world;
            system::Viewport::Id viewport;
            system::Window::Id window;
            scene::Root::Id scene;
            scene::Camera::Id camera;
            resource::ShadowMap::Id shadow_map;
        };

        void render(FrameContext args);

    private:
        void bind_material(FrameContext args, renderer::Pass pass, resource::Material::Id material);
        void bind_instance(FrameContext args, resource::Material::Id material, const renderer::Command& command);
        void bind_lights(FrameContext args, resource::Material::Id material);
        void draw_geometry(fqsm::Reading world, resource::Geometry::Id geometry);
        void execute_command(FrameContext args, const renderer::Command& command);
    };

}
