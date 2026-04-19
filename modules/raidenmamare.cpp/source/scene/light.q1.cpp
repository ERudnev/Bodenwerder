#include <Raidenmamare/scene/light.q1.h>

namespace rmmr::scene {
    auto Light::Operations::create(Writing permit, Pos position, HPB hpb, RGB color, float intensity, float range) -> Id {
        repo::Accumulator transaction(permit);
        const auto node = Node::Operations::create_posHpb(transaction, position, hpb);
        ops::particle::create<Light>(
            transaction,
            node,
            Light::Quantum{
                .color = color,
                .intensity = intensity,
                .range = range,
            }
        );
        return node;
    }

    const Invariants Light::invariants{
        .structural = {},
        .logical = {},
    };
}

