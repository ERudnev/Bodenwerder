#pragma once

#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>
#include <Raidenmamare/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Core : Entity<Core> {
        struct Quantum {
            RGB ambient;
            float ambient_intensity;
        };
        struct Global {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Node_group : Group<Node_group, Core, Node> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Camera_group : Group<Camera_group, Core, Camera> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Light_group : Group<Light_group, Core, Light> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Interface : Manipulation<Interface, Core> {
        static auto createScene(Writing) -> Id;
        static auto createRawNode(Writing, Id, Locator) -> Node::Id;
        static auto createCamera(Writing, Id, Locator, Camera::Quantum) -> Camera::Id;
        static auto createLight(Writing, Id, Locator, Light::Quantum) -> Light::Id;
    };
}
