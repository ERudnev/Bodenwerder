#pragma once

#include "bloom.h"
#include "gl.h"
#include "identity.h"
#include "overlayCompose.h"
#include "sceneTarget.h"

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
        SceneTarget sceneTarget;
        Bloom bloom;
        Identity identity;
        OverlayCompose overlay;
        gl::Triangle fullscreen;
        renderer::UniformBuffer passStateBuffer;
        Stats lastStats;

        void uploadPassState(FrameContext args, maybe<scene::Light::Id> primaryLight);
        void ensure_material(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, PassDrawState& state, maybe<resource::shadow::Runtime::Id> shadow);
        void bindPassResources(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, maybe<resource::shadow::Runtime::Id> shadow);
        void drawGpuBatch(FrameContext args, renderer::Pass pass, const renderer::GpuBatch& batch);
    };

}
