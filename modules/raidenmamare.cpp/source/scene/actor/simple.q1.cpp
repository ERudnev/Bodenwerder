#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/submit.h>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    auto Simple::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, resource::material::Asset::Id material, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Locator{
            .pos = position,
            .euler = hpb,
        });
        with<Simple>::extend(context, node, Simple::Quantum{
            .geometry = geometry,
            .material = material,
            .albedo = albedo,
        });
        return node;
    }

    void Simple::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& actor = with<Simple>::get(context, node);
        submit_material_passes(context, device, DrawInstance{
            .model = Node::Actions::transform(context, node),
            .geometry = actor.geometry,
            .material = actor.material,
            .albedo = actor.albedo,
            .opacity = 1.0f,
        }, where);
    }

}
