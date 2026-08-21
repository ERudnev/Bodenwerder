#pragma once

#include <rmmr/engine.h>
#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/system/window.q1.h>

#include <span>

#include <base/maybe.h>

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    struct PassDrawState {
        base::maybe<resource::shader::Runtime::Id> bound_shader;
        base::maybe<resource::material::Runtime::Id> bound_material;
    };

    class Renderer final {
    public:
        struct Stats {
            integer mdiCalls;
            integer indirectDraws;
        };

        struct FrameContext {
            fqsm::Writing world;
            system::Window::Id window;
            Engine::ViewContext view;
            base::maybe<resource::overlay::Asset::Id> overlay;
            std::span<const renderer::Integer32> selection;
        };

        Renderer();
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void render(FrameContext args);
        auto stats() const -> Stats;

    private:
        struct ColorTarget {
            renderer::Framebuffer fbo;
            renderer::Texture color;
            index2 size;
        };

        struct IdentityTarget {
            renderer::Framebuffer all_fbo;
            renderer::Framebuffer selected_fbo;
            renderer::Texture color;
            renderer::Texture selected;
            renderer::Texture depth;
            index2 size;
        };

        struct SceneTarget {
            renderer::Framebuffer fbo;
            renderer::Texture hdr;
            renderer::Texture bloomMask;
            renderer::Texture depth;
            index2 size;
        };

        struct BloomTarget {
            renderer::Framebuffer sourceFbo;
            renderer::Framebuffer scratchFbo;
            renderer::Texture source;
            renderer::Texture scratch;
            index2 size;
        };

        ColorTarget scene_color_;
        ColorTarget overlay_color_;
        IdentityTarget identity_;
        SceneTarget sceneTarget_;
        BloomTarget bloomTarget_;
        renderer::VertexArray fullscreen_vao_;
        renderer::UniformBuffer passStateBuffer;
        renderer::Program bloomDownsampleProgram_;
        renderer::Program bloomBlurProgram_;
        renderer::Program tonemapProgram_;
        Stats lastStats;

        void ensure_color_target(ColorTarget& target, index2 size, const char* label);
        void ensureSceneTarget(index2 size);
        void beginSceneTarget(index2 size, vec4 clearColor);
        void bindSceneTarget(index2 size);
        void ensureBloomTarget(index2 sceneSize);
        void ensurePostPrograms();
        void downsampleBloom();
        void blurBloom(float radius);
        void tonemapToWindow(FrameContext args, float intensity);
        void ensure_identity_target(index2 size);
        void clear_identity_feature(index2 size);
        void begin_identity_selected_pass(index2 size);
        void begin_identity_pass(index2 size);
        auto peek_identity_under(FrameContext args, index2 viewport_size) -> renderer::Integer32;
        void end_identity_pass(FrameContext args);
        void publish_identity(FrameContext args, integer draws, renderer::Integer32 under);
        void capture_scene_color(index2 size);
        void run_overlay(FrameContext args, index2 size);
        void compose_overlay(index2 size);

        void uploadPassState(FrameContext args, maybe<scene::Light::Id> primaryLight);
        void ensure_material(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, PassDrawState& state, maybe<resource::shadow::Runtime::Id> shadow);
        void bindPassResources(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, maybe<resource::shadow::Runtime::Id> shadow);
        void drawGpuBatch(FrameContext args, renderer::Pass pass, const renderer::GpuBatch& batch);
    };

}
