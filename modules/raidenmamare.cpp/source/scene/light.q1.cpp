#include <Raidenmamare/scene/light.q1.h>

namespace rmmr::scene {

    auto Light::Actions::create(Writing context, Locator locator, RGB color, float intensity, float range) -> Id {
        const auto node = with<Node>::create(context, locator);
        with<Light>::extend(context, node, Quantum{.color = color, .intensity = intensity, .range = range});
        return node;
    }
}
