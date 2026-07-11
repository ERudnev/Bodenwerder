#include <Raidenmamare/scene/root.q1.h>

#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        auto node_quantum_from_locator(Locator locator) -> Node::Quantum {
            const vec3 radians = glm::radians(locator.euler);
            const glm::vec3 glm_euler{
                radians.y,
                radians.x,
                radians.z,
            };
            return Node::Quantum{
                .position = locator.pos,
                .rotation = glm::normalize(quat{glm_euler}),
            };
        }

    } // namespace

    auto Interface::createScene(Writing context) -> Root::Id {
        const auto root = with<Root>::create(context, Root::Quantum{
            .ambient = RGB{0.4f, 0.4f, 0.4f},
            .ambient_intensity = 0.8f,
        });

        with<Node_group>::extend(context, root);
        with<Camera_group>::extend(context, root);
        with<Light_group>::extend(context, root);

        return root;
    }

    auto Interface::createCamera(Writing context, Root::Id root, Locator locator, Camera::Quantum cameraQuantum) -> Camera::Id {
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<Camera_group>::addElement(context, root, node, std::move(cameraQuantum));
        return node;
    }

    auto Interface::createLight(Writing context, Root::Id root, Locator locator, Light::Quantum lightQuantum) -> Light::Id {
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<Light_group>::addElement(context, root, node, std::move(lightQuantum));
        return node;
    }

    auto Interface::createPrimitiveActor(Writing context, Root::Id root, Locator locator, PrimitiveActor::Quantum actorQuantum) -> PrimitiveActor::Id {
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<PrimitiveActor>::extend(context, node, std::move(actorQuantum));
        return node;
    }

}
