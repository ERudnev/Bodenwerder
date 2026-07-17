#include <rmmr/scene/actor.q1.h>
#include <rmmr/resources/runtimes.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto PrimitiveActor::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, resource::material::Asset::Id material, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Locator{
            .pos = position,
            .euler = hpb,
        });
        with<PrimitiveActor>::extend(context, node, PrimitiveActor::Quantum{
            .geometry = geometry,
            .material = material,
            .albedo = albedo,
        });
        return node;
    }

    void PrimitiveActor::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& actor = with<PrimitiveActor>::get(context, node);
        const auto& global = PrimitiveActor::Actions::get_global(context);
        const auto& runtimes = with<resource::Runtimes>::get(context, device);
        const auto model = Node::Actions::transform(context, node);

        const auto geometry_it = runtimes.geometries_id_mapping.find(actor.geometry);
        const auto material_it = runtimes.materials_id_mapping.find(actor.material);
        if (geometry_it == runtimes.geometries_id_mapping.end() || material_it == runtimes.materials_id_mapping.end()) {
            return;
        }

        where.push_back(renderer::Command{
            .pass = renderer::Pass::opaque,
            .model = model,
            .geometry = geometry_it->second,
            .material = material_it->second,
            .albedo = actor.albedo,
            .opacity = 1.0f,
            .instance_data = {},
            .instance_count = renderer::Count{1},
            .render_state = {},
        });

        if (not global.shadowMaterial) {
            return;
        }

        const auto shadow_it = runtimes.materials_id_mapping.find(*global.shadowMaterial);
        if (shadow_it == runtimes.materials_id_mapping.end()) {
            return;
        }

        where.push_back(renderer::Command{
            .pass = renderer::Pass::shadow,
            .model = model,
            .geometry = geometry_it->second,
            .material = shadow_it->second,
            .albedo = actor.albedo,
            .opacity = 1.0f,
            .instance_data = {},
            .instance_count = renderer::Count{1},
            .render_state = {},
        });
    }

}
