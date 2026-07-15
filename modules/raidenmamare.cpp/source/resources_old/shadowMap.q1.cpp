#include <rmmr/resources_old/shadowMap.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace rmmr::resource_old {

    using namespace fqsm::api;
    using namespace api_for_internals;

    namespace {

        auto device_for_shadow_map(Reading context, ShadowMap::Id shadow_map) -> system::Device::Id {
            for (const auto entry : context->aspect<ShadowMap_group>().items()) {
                if (entry.value.contains(shadow_map)) {
                    return entry.id;
                }
            }
            throw std::runtime_error("ShadowMap: shadow map is not attached to a device");
        }

    } // namespace

    auto ShadowMap::Actions::create(Writing context, system::Device::Id device, index2 size) -> Id {
        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);

        DepthTexture depth{};
        glGenTextures(1, &depth);
        glBindTexture(GL_TEXTURE_2D, depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const GLfloat border_color[]{1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

        Framebuffer fbo{};
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("resource_old::ShadowMap::create: framebuffer incomplete");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        with<ShadowMap_group>::extend(context, device);
        return with<ShadowMap_group>::addElement(context, device, Quantum{
            .fbo = fbo,
            .depth = depth,
            .size = size,
        });
    }

    void ShadowMap::Actions::bind(Reading context, Id shadow_map) {
        const auto device = device_for_shadow_map(context, shadow_map);
        glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);

        const auto& quantum = with<ShadowMap>::get(context, shadow_map);
        glBindFramebuffer(GL_FRAMEBUFFER, quantum.fbo);

        const int width = std::max(static_cast<int>(quantum.size.x), 1);
        const int height = std::max(static_cast<int>(quantum.size.y), 1);
        glViewport(0, 0, width, height);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
    }

    void ShadowMap::Actions::clear(Reading context, Id shadow_map) {
        const auto device = device_for_shadow_map(context, shadow_map);
        glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void ShadowMap::Actions::unbind(Reading context, Id) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

}
