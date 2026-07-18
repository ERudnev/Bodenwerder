#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <base/maybe.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    struct PassDrawState {
        base::maybe<resource::shader::Runtime::Id> bound_shader;
        base::maybe<resource::material::Runtime::Id> bound_material;
        base::maybe<resource::geometry::Runtime::Id> bound_geometry;
    };

    class Renderer final {
    public:
        struct FrameContext {
            fqsm::Reading world;
            system::Viewport::Id viewport;
            system::Window::Id window;
            scene::Root::Id scene;
            scene::Camera::Id camera;
        };

        void render(FrameContext args);

    private:
        void ensure_material(
            FrameContext args,
            renderer::Pass pass,
            resource::material::Runtime::Id material,
            PassDrawState& state,
            scene::Light::Id primary_light,
            base::maybe<resource::shadow::Runtime::Id> shadow,
            scene::Light::Id shadow_space_light);
        void bind_pass_uniforms(
            FrameContext args,
            renderer::Pass pass,
            resource::material::Runtime::Id material,
            scene::Light::Id primary_light,
            base::maybe<resource::shadow::Runtime::Id> shadow,
            scene::Light::Id shadow_space_light);
        void bind_material_samplers(FrameContext args, resource::material::Runtime::Id material);
        void draw_instance(FrameContext args, const renderer::Command& command, resource::material::Runtime::Id material);
        void draw_stats_overlay(FrameContext args);
    };

}
