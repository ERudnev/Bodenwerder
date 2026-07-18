#include <rmmr/scene/submit.h>

#include <rmmr/resources/runtimes.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    void submit_material_passes(Reading context, system::Device::Id device, const DrawInstance& draw, renderer::CommandBuffer& where) {
        const auto& runtimes = with<resource::Runtimes>::get(context, device);

        const auto geometry_it = runtimes.geometries_id_mapping.find(draw.geometry);
        const auto material_it = runtimes.materials_id_mapping.find(draw.material);
        if (geometry_it == runtimes.geometries_id_mapping.end() || material_it == runtimes.materials_id_mapping.end()) {
            return;
        }

        const auto& material = with<resource::material::Runtime>::get(context, material_it->second);

        for (const auto pass : material.passes) {
            auto command_material = material_it->second;
            if (pass == renderer::Pass::shadow) {
                if (not draw.shadow_material) {
                    continue;
                }
                const auto shadow_it = runtimes.materials_id_mapping.find(*draw.shadow_material);
                if (shadow_it == runtimes.materials_id_mapping.end()) {
                    continue;
                }
                command_material = shadow_it->second;
            }

            where.push_back(renderer::Command{
                .pass = pass,
                .model = draw.model,
                .geometry = geometry_it->second,
                .material = command_material,
                .albedo = draw.albedo,
                .opacity = draw.opacity,
                .instance_data = {},
                .instance_count = renderer::Count{1},
                .render_state = {},
            });
        }
    }

}
