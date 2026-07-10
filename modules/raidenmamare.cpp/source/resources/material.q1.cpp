#include <Raidenmamare/resources/material.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    void Material::Actions::apply(Reading context, Id material, system::Device::Id device) {
        const auto& quantum = with<Material>::get(context, material);
        const auto& shader_quantum = with<Shader>::get(context, quantum.shader);
        const auto& device_quantum = with<system::Device>::get(context, device);

        glfwMakeContextCurrent(device_quantum.handle);
        glUseProgram(shader_quantum.handle);
    }

}
