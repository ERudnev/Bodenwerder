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

        for (const auto& [pass, technique] : material.techniques) {
            where.push_back(renderer::Command{
                .pass = pass,
                .model = draw.model,
                .geometry = geometry_it->second,
                .material = material_it->second,
                .shader = technique.shader,
                .albedo = draw.albedo,
                .opacity = draw.opacity,
                .instance_data = {},
                .instance_count = renderer::Count{1},
                .render_state = {},
            });
        }
    }

}
