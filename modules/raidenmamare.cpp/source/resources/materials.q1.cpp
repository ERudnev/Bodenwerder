#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace rmmr::resource::material {

    using namespace fqsm::api;

    auto Asset::Always::uniformIds(const vector<string>& names) -> Uniform::Palette {
        Uniform::Palette out;
        out.reserve(names.size());

        for (const auto& name : names) {
            const auto id = ::rmmr::material::Semantics::id_of(name);
            if (id == ::rmmr::material::Semantics::PersistentId{0}) {
                throw std::runtime_error("resource::material::Asset::uniformIds: unknown uniform semantic: " + name);
            }
            out.push_back(id);
        }

        return out;
    }

    auto Composed::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> Runtime::Quantum {
        const auto& asset = with<Asset>::get(context, asset_id);
        const auto& runtimes = with<Runtimes>::get(context, device);

        const auto shader_it = runtimes.shaders_id_mapping.find(asset.program);
        if (shader_it == runtimes.shaders_id_mapping.end()) {
            return context.refuse("resource::material::Composed::materialize: shader runtime missing for material asset");
        }

        const auto& shader_quantum = with<shader::Runtime>::get(context, shader_it->second);
        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        Runtime::Locations locations{};
        locations.reserve(asset.uniforms.size());
        vector<Uniform::Binding> bindings{};
        bindings.reserve(asset.uniforms.size());
        vector<Runtime::TextureBinding> textures{};
        textures.reserve(asset.textures.size());

        for (const auto persistent_id : asset.uniforms) {
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

        for (const auto& texture_binding : asset.textures) {
            const auto texture_it = runtimes.textures_id_mapping.find(texture_binding.texture);
            if (texture_it == runtimes.textures_id_mapping.end()) {
                return context.refuse("resource::material::Composed::materialize: texture runtime missing for material asset");
            }
            textures.push_back(Runtime::TextureBinding{
                .id = texture_binding.id,
                .texture = texture_it->second,
            });
        }

        return Runtime::Quantum{
            .shader = shader_it->second,
            .locations = std::move(locations),
            .bindings = std::move(bindings),
            .textures = std::move(textures),
        };
    }

    void Runtime::Actions::apply(Reading context, Id material, system::Device::Id) {
        const auto& quantum = with<Runtime>::get(context, material);
        const auto& shader_quantum = with<shader::Runtime>::get(context, quantum.shader);
        glUseProgram(shader_quantum.handle);
    }

}
