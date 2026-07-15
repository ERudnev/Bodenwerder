#include <rmmr/scene/actor.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto PrimitiveActor::Actions::create(Writing context, Pos position, HPB hpb, resource_old::Geometry::Id geometry, resource_old::Material::Id material, RGB albedo) -> Id {
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

    void PrimitiveActor::Actions::submit(Reading context, Id node, renderer::CommandBuffer& where) {
        const auto& actor = with<PrimitiveActor>::get(context, node);
        const auto& global = PrimitiveActor::Actions::get_global(context);
        const auto model = Node::Actions::transform(context, node);

        where.push_back(renderer::Command{
            .pass = renderer::Pass::opaque,
            .model = model,
            .geometry = actor.geometry,
            .material = actor.material,
            .albedo = actor.albedo,
            .opacity = 1.0f,
            .instance_data = {},
            .instance_count = renderer::Count{1},
            .render_state = {},
        });
        if (global.shadowMaterial) {
            where.push_back(renderer::Command{
                .pass = renderer::Pass::shadow,
                .model = model,
                .geometry = actor.geometry,
                .material = *global.shadowMaterial,
                .albedo = actor.albedo,
                .opacity = 1.0f,
                .instance_data = {},
                .instance_count = renderer::Count{1},
                .render_state = {},
            });
        }
    }

}
