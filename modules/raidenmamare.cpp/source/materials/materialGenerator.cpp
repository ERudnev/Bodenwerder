#include "materialGenerator.h"

#include <utility>

namespace rmmr::material {
    using namespace api_for_internals;
    namespace {
        resource::Material::Id create_preset(
            Writing context,
            system::Device::Id device,
            string program_name,
            string material_name,
            string library,
            asset::Uniform::Palette uniforms,
            vector<asset::Material::TextureBinding> textures = {}
        ) {
            const auto asset_shader = with<asset::Shader>::create(context, asset::Shader::Quantum{
                .name = std::move(program_name),
                .library = std::move(library),
            });
            const auto asset_material = with<asset::Material>::create(context, asset::Material::Quantum{
                .name = std::move(material_name),
                .program = asset_shader,
                .uniforms = std::move(uniforms),
                .textures = std::move(textures),
            });
            return asset::Material::Actions::compile(context, asset_material, device);
        }
    } // namespace

    auto MaterialGenerator::ambient(Writing context, system::Device::Id device) -> resource::Material::Id {
        return create_preset(context, device, "ambient", "ambient", "rmmr",
            asset::Material::Always::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "albedo",
                "ambientColor",
                "ambientIntensity",
            }));
    }

    auto MaterialGenerator::lit(Writing context, system::Device::Id device) -> resource::Material::Id {
        return create_preset(context, device, "lit", "lit", "rmmr",
            asset::Material::Always::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "albedo",
                "ambientColor",
                "ambientIntensity",
                "light0Pos",
                "light0Color",
                "light0Intensity",
                "lightSpaceMatrix",
                "shadowMap",
            }));
    }

    auto MaterialGenerator::litTextured(Writing context, system::Device::Id device, asset::Texture::Id albedo_map) -> resource::Material::Id {
        return create_preset(context, device, "litTextured", "litTextured", "rmmr",
            asset::Material::Always::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "albedo",
                "albedoMap",
                "ambientColor",
                "ambientIntensity",
                "light0Pos",
                "light0Color",
                "light0Intensity",
                "lightSpaceMatrix",
                "shadowMap",
            }),
            vector<asset::Material::TextureBinding>{
                asset::Material::TextureBinding{
                    .id = material::Semantics::id_of("albedoMap"),
                    .texture = albedo_map,
                },
            });
    }

    auto MaterialGenerator::grid(fqsm::Writing context, system::Device::Id device) -> resource::Material::Id {
        return create_preset(context, device, "Grid", "grid", "rmmr",
            asset::Material::Always::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "patternScale",
                "colorPrimary",
                "colorSecondary",
            }));
    }

    auto MaterialGenerator::shadowDepth(fqsm::Writing context, system::Device::Id device) -> resource::Material::Id {
        return create_preset(context, device, "shadowDepth", "shadowDepth", "rmmr",
            asset::Material::Always::uniformIds(vector<string>{
                "model",
                "lightSpaceMatrix",
            }));
    }
}
