#include <Raidenmamare/scene/actor.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto PrimitiveActor::Actions::create(Writing context, Pos position, HPB hpb, resource::Geometry::Id geometry, resource::Material::Id material, RGB albedo) -> Id {
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

}
