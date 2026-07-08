#include <Raidenmamare/scene/core.q1.h>

namespace rmmr::scene {

    auto Interface::createScene(Writing context) -> Id {
        const auto core = with<Core>::create(context, Quantum{
            .ambient = RGB{0.4f, 0.4f, 0.4f},
            .ambient_intensity = 0.8f,
        });
        with<Node_group>::extend(context, core);
        with<Camera_group>::extend(context, core);
        with<Light_group>::extend(context, core);
        return core;
    }

    auto Interface::createRawNode(Writing context, Id core, Locator locator) -> Node::Id {
        const auto node = with<Node>::create(context, locator);
        with<Node_group>::modify(context, core)->insert(node);
        return node;
    }

    auto Interface::createCamera(Writing context, Id core, Locator locator, Camera::Quantum cameraQuantum) -> Camera::Id {
        const auto node = createRawNode(context, core, locator);
        with<Camera_group>::addElement(context, core, node, cameraQuantum);
        return node;
    }

    auto Interface::createLight(Writing context, Id core, Locator locator, Light::Quantum lightQuantum) -> Light::Id {
        const auto node = createRawNode(context, core, locator);
        with<Light_group>::addElement(context, core, node, lightQuantum);
        return node;
    }
}
