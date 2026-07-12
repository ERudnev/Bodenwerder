#include <rmmr/resources/shadowMap.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

namespace rmmr::resource {

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
