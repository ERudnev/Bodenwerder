#include "materialGenerator.h"

#include <utility>

namespace rmmr::material {
    namespace {

        resource::Material::Id create_preset(fqsm::Writing context, system::Device::Id device, string program_name, string core_name, string library, asset::Uniform::Palette uniforms) {
            const auto asset_shader = with<asset::Shader>::create(context, asset::Shader::Quantum{
                .name = std::move(program_name),
                .library = std::move(library),
            });
            const auto asset_material = with<asset::Material>::create(context, asset::Material::Quantum{
                .name = std::move(core_name),
                .program = asset_shader,
                .uniforms = std::move(uniforms),
            });
            return asset::Material::Actions::compile(context, asset_material, device);
        }

    } // namespace

    auto MaterialGenerator::ambient(fqsm::Writing context, system::Device::Id device) -> resource::Material::Id {
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

    auto MaterialGenerator::lit(fqsm::Writing context, system::Device::Id device) -> resource::Material::Id {
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
            }));
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
}
