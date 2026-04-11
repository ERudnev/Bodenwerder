#include <Raidenmamare/scene/light.q1.h>

namespace rmmr::scene {
    auto Light::Operations::create(Writing commit, Pos position, HPB hpb, RGB color, float intensity, float range) -> Id {
        const auto node = Node::Operations::create_posHpb(commit, position, hpb);
        ops::particle::create<Light>(
            commit,
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

