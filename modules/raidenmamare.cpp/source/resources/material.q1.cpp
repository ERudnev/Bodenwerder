#include <rmmr/resources/material.q1.h>

#include <GL/glew.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    void Material::Actions::apply(Reading context, Id material, system::Device::Id device) {
        const auto& quantum = with<Material>::get(context, material);
        const auto& shader_quantum = with<Shader>::get(context, quantum.shader);
        glUseProgram(shader_quantum.handle);
    }

}
