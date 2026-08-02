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
        base::maybe<resource::sprite::Runtime::Id> sprite{};
        integer sprite_index = 0;
        if (draw.sprite) {
            const auto sprite_it = runtimes.sprites_id_mapping.find(draw.sprite->pack);
            if (sprite_it == runtimes.sprites_id_mapping.end()) {
                return;
            }
            sprite = sprite_it->second;
            sprite_index = draw.sprite->index;
        }

        const auto& material = with<resource::material::Runtime>::get(context, material_it->second);

        for (const auto& [pass, technique] : material.techniques) {
            base::maybe<renderer::Command::IndexRange> indices{};
            if (draw.indices) {
                indices = renderer::Command::IndexRange{.start = draw.indices->start, .count = draw.indices->count};
            }
            where[pass].push_back(renderer::Command{
                .model = draw.model,
                .geometry = geometry_it->second,
                .material = material_it->second,
                .shader = technique.shader,
                .sprite = sprite,
                .sprite_index = sprite_index,
                .albedo = draw.albedo,
                .opacity = draw.opacity,
                .instance_data = {},
                .instance_count = renderer::Count{1},
                .render_state = renderer::RenderState{.blend = material.blend},
                .indices = indices,
            });
        }
    }

}
