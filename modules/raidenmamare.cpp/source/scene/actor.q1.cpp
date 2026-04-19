#include <Raidenmamare/scene/actor.q1.h>

namespace rmmr::scene {
    auto PrimitiveActor::Operations::create(
        Writing permit,
        Pos position,
        HPB hpb,
        primitive::Base::Id geometry,
        material::Core::Id material,
        RGB albedo) -> Id
    {
        repo::Accumulator transaction(permit);
        const auto node = Node::Operations::create_posHpb(transaction, position, hpb);
        ops::particle::create<PrimitiveActor>(
            transaction,
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
