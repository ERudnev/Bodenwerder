#include <rmmr/resources/overlays.q1.h>

#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace rmmr::resource::overlay {

    using namespace fqsm::api;

    auto Asset::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& asset = with<Asset>::get(context, asset_id);
        const auto& runtimes = with<Runtimes>::get(context, device);

        const auto shader_it = runtimes.shaders_id_mapping.find(asset.program.id);
        if (shader_it == runtimes.shaders_id_mapping.end()) {
            return context.refuse("resource::overlay::Asset::materialize: shader runtime missing");
        }

        const auto& shader_quantum = with<shader::Runtime>::get(context, shader_it->second);
        glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);

        vector<Uniform::Binding> bindings{};
        bindings.reserve(asset.uniforms.size());
        for (const auto persistent_id : asset.uniforms) {
            const auto semantic_name = ::rmmr::material::Semantics::name_of(persistent_id);
            if (semantic_name == ::rmmr::material::Semantics::Name{"_undefined"}) {
                bindings.push_back(Uniform::Binding{.id = persistent_id, .type = ::rmmr::material::Semantics::type_of(persistent_id), .location = GLint{-1}});
                continue;
            }
            const auto uniform_name = ::rmmr::material::Semantics::uniform_name(semantic_name);
            const auto location = glGetUniformLocation(shader_quantum.handle, uniform_name.c_str());
            bindings.push_back(Uniform::Binding{
                .id = persistent_id,
                .type = ::rmmr::material::Semantics::type_of(persistent_id),
                .location = location,
            });
        }

        Runtime::Quantum quantum{
            .shader = shader_it->second,
            .bindings = std::move(bindings),
            .scale = asset.scale,
        };

        if (const auto existing = runtimes.overlays_id_mapping.find(asset_id); existing != runtimes.overlays_id_mapping.end()) {
            if (with<Runtime>::exists(context, existing->second)) {
                *with<Runtime>::modify(context, existing->second) = std::move(quantum);
                return existing->second;
            }
        }

        return with<OverlayRuntime_group>::addElement(context, device, std::move(quantum));
    }

}
