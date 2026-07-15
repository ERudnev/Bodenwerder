#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources_old/material.q1.h>
#include <rmmr/resources_old/shadowMap.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <base/maybe.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    struct PassDrawState {
        base::maybe<resource_old::Material::Id> bound_material;
        base::maybe<resource_old::Geometry::Id> bound_geometry;
    };

    class Renderer final {
    public:
        struct FrameContext {
            fqsm::Reading world;
            system::Viewport::Id viewport;
            system::Window::Id window;
            scene::Root::Id scene;
            scene::Camera::Id camera;
            resource_old::ShadowMap::Id shadow_map;
        };

        void render(FrameContext args);

    private:
        void ensure_material(FrameContext args, renderer::Pass pass, resource_old::Material::Id material, PassDrawState& state);
        void bind_pass_uniforms(FrameContext args, renderer::Pass pass, resource_old::Material::Id material);
        void draw_instance(FrameContext args, const renderer::Command& command, resource_old::Material::Id material);
        void draw_stats_overlay(FrameContext args);
    };

}
