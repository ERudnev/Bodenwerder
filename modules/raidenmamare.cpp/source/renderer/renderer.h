#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/material.q1.h>
#include <rmmr/resources/shadowMap.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <base/maybe.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    struct PassDrawState {
        base::maybe<resource::Material::Id> bound_material;
        base::maybe<resource::Geometry::Id> bound_geometry;
    };

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
        void ensure_material(FrameContext args, renderer::Pass pass, resource::Material::Id material, PassDrawState& state);
        void bind_pass_uniforms(FrameContext args, renderer::Pass pass, resource::Material::Id material);
        void draw_instance(FrameContext args, const renderer::Command& command, resource::Material::Id material);
    };

}
