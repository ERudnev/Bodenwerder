#include <Raidenmamare/assets/material.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace rmmr::asset {

    using namespace fqsm::api;

    namespace {

        auto create_resource_quantum(Writing context, const Material::Quantum& asset, system::Device::Id device) -> resource::Material::Quantum {
            const auto resource_shader = Shader::Actions::compile(context, asset.program, device);
            const auto& shader_quantum = with<resource::Shader>::get(context, resource_shader);
            const auto& device_quantum = with<system::Device>::get(context, device);

            glfwMakeContextCurrent(device_quantum.handle);

            resource::Material::Locations locations{};
            locations.reserve(asset.uniforms.size());
            vector<Uniform::Binding> bindings{};
            bindings.reserve(asset.uniforms.size());

            for (const auto persistent_id : asset.uniforms) {
                const auto semantic_name = material::Semantics::name_of(persistent_id);
                if (semantic_name == material::Semantics::Name{"_undefined"}) {
                    locations.emplace(persistent_id, GLint{-1});
                    bindings.push_back(Uniform::Binding{
                        .id = persistent_id,
                        .type = material::Semantics::type_of(persistent_id),
                        .location = GLint{-1},
                    });
                    continue;
                }

                const auto uniform_name = material::Semantics::uniform_name(semantic_name);
                const auto location = glGetUniformLocation(shader_quantum.handle, uniform_name.c_str());
                locations.emplace(persistent_id, location);
                bindings.push_back(Uniform::Binding{
                    .id = persistent_id,
                    .type = material::Semantics::type_of(persistent_id),
                    .location = location,
                });
            }

            return resource::Material::Quantum{
                .shader = resource_shader,
                .locations = std::move(locations),
                .bindings = std::move(bindings),
            };
        }

    } // namespace

    auto Material::Always::uniformIds(const vector<string>& names) -> Uniform::Palette {
        Uniform::Palette out;
        out.reserve(names.size());

        for (const auto& name : names) {
            const auto id = material::Semantics::id_of(name);
            if (id == material::Semantics::PersistentId{0}) {
                throw std::runtime_error("asset::Material::uniformIds: unknown uniform semantic: " + name);
            }
            out.push_back(id);
        }

        return out;
    }

    auto Material::Actions::compile(Writing context, Id asset_material, system::Device::Id device) -> resource::Material::Id {
        const auto& asset = with<Material>::get(context, asset_material);
        with<resource::Material_group>::extend(context, device);
        return with<resource::Material_group>::addElement(context, device, create_resource_quantum(context, asset, device));
    }

}
