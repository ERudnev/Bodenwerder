#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace rmmr::resource::material {

    using namespace fqsm::api;

    namespace {

        auto materialize_technique(
            Writing context,
            const Asset::Technique& asset_technique,
            const Runtimes::Quantum& runtimes,
            system::Device::Id device) -> optional<Runtime::Technique>
        {
            const auto shader_it = runtimes.shaders_id_mapping.find(asset_technique.program);
            if (shader_it == runtimes.shaders_id_mapping.end()) {
                return context.refuse("resource::material::Composer::materialize: shader runtime missing for technique");
            }

            const auto& shader_quantum = with<shader::Runtime>::get(context, shader_it->second);
            const auto& device_quantum = with<system::Device>::get(context, device);
            glfwMakeContextCurrent(device_quantum.handle);

            Runtime::Locations locations{};
            locations.reserve(asset_technique.uniforms.size());
            vector<Uniform::Binding> bindings{};
            bindings.reserve(asset_technique.uniforms.size());
            vector<Runtime::TextureBinding> textures{};
            textures.reserve(asset_technique.textures.size());

            for (const auto persistent_id : asset_technique.uniforms) {
                const auto semantic_name = ::rmmr::material::Semantics::name_of(persistent_id);
                if (semantic_name == ::rmmr::material::Semantics::Name{"_undefined"}) {
                    locations.emplace(persistent_id, GLint{-1});
                    bindings.push_back(Uniform::Binding{
                        .id = persistent_id,
                        .type = ::rmmr::material::Semantics::type_of(persistent_id),
                        .location = GLint{-1},
                    });
                    continue;
                }

                const auto uniform_name = ::rmmr::material::Semantics::uniform_name(semantic_name);
                const auto location = glGetUniformLocation(shader_quantum.handle, uniform_name.c_str());
                locations.emplace(persistent_id, location);
                bindings.push_back(Uniform::Binding{
                    .id = persistent_id,
                    .type = ::rmmr::material::Semantics::type_of(persistent_id),
                    .location = location,
                });
            }

            for (const auto& texture_binding : asset_technique.textures) {
                const auto texture_it = runtimes.textures_id_mapping.find(texture_binding.texture);
                if (texture_it == runtimes.textures_id_mapping.end()) {
                    return context.refuse("resource::material::Composer::materialize: texture runtime missing for technique");
                }
                textures.push_back(Runtime::TextureBinding{
                    .id = texture_binding.id,
                    .texture = texture_it->second,
                });
            }

            return Runtime::Technique{
                .shader = shader_it->second,
                .locations = std::move(locations),
                .bindings = std::move(bindings),
                .textures = std::move(textures),
            };
        }

    } // namespace

    auto Composer::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& asset = with<Asset>::get(context, asset_id);
        const auto& runtimes = with<Runtimes>::get(context, device);

        umap<renderer::Pass, Runtime::Technique> techniques{};
        for (const auto& [pass, asset_technique] : asset.techniques) {
            auto technique = materialize_technique(context, asset_technique, runtimes, device);
            if (not technique) {
                return {};
            }
            techniques.emplace(pass, std::move(*technique));
        }

        Runtime::Quantum quantum{
            .techniques = std::move(techniques),
        };

        if (const auto existing = runtimes.materials_id_mapping.find(asset_id); existing != runtimes.materials_id_mapping.end()) {
            if (with<Runtime>::exists(context, existing->second)) {
                *with<Runtime>::modify(context, existing->second) = std::move(quantum);
                return existing->second;
            }
        }

        return with<MaterialRuntime_group>::addElement(context, device, std::move(quantum));
    }

    void Runtime::Actions::apply(Reading context, Id material, system::Device::Id, renderer::Pass pass) {
        const auto& quantum = with<Runtime>::get(context, material);
        const auto technique_it = quantum.techniques.find(pass);
        if (technique_it == quantum.techniques.end()) {
            throw std::runtime_error("resource::material::Runtime::apply: no technique for pass");
        }
        const auto& shader_quantum = with<shader::Runtime>::get(context, technique_it->second.shader);
        glUseProgram(shader_quantum.handle);
    }

}
