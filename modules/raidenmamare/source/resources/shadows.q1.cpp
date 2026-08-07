#include <rmmr/resources/shadows.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>

namespace rmmr::resource::shadow {

    using namespace fqsm::api;

    namespace {

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (not last.fbo and not last.depth) {
                return;
            }
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            if (last.fbo) {
                auto fbo = last.fbo;
                glDeleteFramebuffers(1, &fbo);
            }
            if (last.depth) {
                auto depth = last.depth;
                glDeleteTextures(1, &depth);
            }
        }

        auto install_runtime(Writing context, system::Device::Id device, Asset::Id asset_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.shadows_id_mapping.find(asset_id); existing != runtimes.shadows_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<ShadowRuntime_group>::addElement(context, device, std::move(quantum));
        }

    } // namespace

    auto Allocator::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& allocator = with<Allocator>::get(context, asset_id);
        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        const int width = std::max(static_cast<int>(allocator.size.x), 1);
        const int height = std::max(static_cast<int>(allocator.size.y), 1);

        renderer::Texture depth{};
        glCreateTextures(GL_TEXTURE_2D, 1, &depth);
        if (not depth) {
            return context.refuse("resource::shadow::Allocator::materialize: glCreateTextures failed");
        }

        glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT24, width, height);
        glTextureParameteri(depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const GLfloat border_color[]{1.0f, 1.0f, 1.0f, 1.0f};
        glTextureParameterfv(depth, GL_TEXTURE_BORDER_COLOR, border_color);

        renderer::Framebuffer fbo{};
        glCreateFramebuffers(1, &fbo);
        if (not fbo) {
            glDeleteTextures(1, &depth);
            return context.refuse("resource::shadow::Allocator::materialize: glCreateFramebuffers failed");
        }

        glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);
        glNamedFramebufferDrawBuffer(fbo, GL_NONE);
        glNamedFramebufferReadBuffer(fbo, GL_NONE);

        if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glDeleteFramebuffers(1, &fbo);
            glDeleteTextures(1, &depth);
            return context.refuse("resource::shadow::Allocator::materialize: framebuffer incomplete");
        }

        return install_runtime(context, device, asset_id, Runtime::Quantum{
            .device = device,
            .fbo = fbo,
            .depth = depth,
            .size = allocator.size,
        });
    }

    void Runtime::Actions::bind(Reading context, Id shadow_map) {
        const auto& quantum = with<Runtime>::get(context, shadow_map);
        glfwMakeContextCurrent(with<system::Device>::get(context, quantum.device).handle);

        glBindFramebuffer(GL_FRAMEBUFFER, quantum.fbo);

        const int width = std::max(static_cast<int>(quantum.size.x), 1);
        const int height = std::max(static_cast<int>(quantum.size.y), 1);
        glViewport(0, 0, width, height);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
    }

    void Runtime::Actions::clear(Reading context, Id shadow_map) {
        const auto& quantum = with<Runtime>::get(context, shadow_map);
        glfwMakeContextCurrent(with<system::Device>::get(context, quantum.device).handle);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void Runtime::Actions::unbind(Reading, Id) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            release_gl(context, last);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
