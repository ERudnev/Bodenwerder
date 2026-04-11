#include <Raidenmamare/scene/actor.q1.h>

namespace rmmr::scene {
    auto PrimitiveActor::Operations::create(
        Writing commit,
        Pos position,
        HPB hpb,
        primitive::Base::Id geometry,
        material::Core::Id material,
        RGB albedo) -> Id
    {
        const auto node = Node::Operations::create_posHpb(commit, position, hpb);
        ops::particle::create<PrimitiveActor>(
            commit,
            node,
            PrimitiveActor::Quantum{
                .geometry = geometry,
                .material = material,
                .albedo = albedo,
            }
        );
        return node;
    }

    const Invariants PrimitiveActor::invariants{
        .structural = {
            invariant::anchor_attribute<Node, PrimitiveActor>,
        },
        .logical = {},
    };
}
