#include <rmmr/scene/light.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto Light::Actions::create(Writing context, Pose pose, Kind kind, RGB color, float intensity, float range) -> Id {
        const auto node = Node::Actions::create(context, pose);
        with<Light>::extend(context, node, Light::Quantum{
            .kind = kind,
            .color = color,
            .intensity = intensity,
            .range = range,
        });
        return node;
    }

}
