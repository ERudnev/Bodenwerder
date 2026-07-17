#include <rmmr/scene/light.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto Light::Actions::create(Writing context, Locator locator, RGB color, float intensity, float range) -> Id {
        const auto node = Node::Actions::create(context, locator);
        with<Light>::extend(context, node, Light::Quantum{
            .color = color,
            .intensity = intensity,
            .range = range,
            .shadow = {},
        });
        return node;
    }

}
