#pragma once

#include <rmmr/engine.h>
#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/scene/light.q1.h>
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
            fqsm::Writing world;
            system::Window::Id window;
            Engine::ViewContext view;
        };

        Renderer();
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void render(FrameContext args);

    private:
        struct IdentityTarget {
            renderer::Framebuffer fbo;
            renderer::Texture color;
            renderer::Texture depth;
            index2 size;
        };

        IdentityTarget identity_;

        void ensure_identity_target(index2 size);
        void begin_identity_pass(index2 size);
        auto peek_identity_under(FrameContext args, index2 viewport_size) -> renderer::Integer32;
        void end_identity_pass(FrameContext args);
        void publish_identity(FrameContext args, integer draws, renderer::Integer32 under);

        void ensure_material(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, PassDrawState& state, maybe<scene::Light::Id> primary_light, maybe<resource::shadow::Runtime::Id> shadow);
        void bind_pass_uniforms(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, maybe<scene::Light::Id> primary_light, maybe<resource::shadow::Runtime::Id> shadow);
        void bind_material_samplers(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material);
        void draw_instance(FrameContext args, renderer::Pass pass, const renderer::Command& command, resource::material::Runtime::Id material);
    };

}
