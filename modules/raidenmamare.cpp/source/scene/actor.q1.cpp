#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/submit.h>

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
        submit_material_passes(context, device, DrawInstance{
            .model = Node::Actions::transform(context, node),
            .geometry = actor.geometry,
            .material = actor.material,
            .albedo = actor.albedo,
            .opacity = 1.0f,
            .shadow_material = PrimitiveActor::Actions::get_global(context).shadowMaterial,
        }, where);
    }

}
