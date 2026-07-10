#pragma once

#include <Raidenmamare/scene/actor.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>
#include <Raidenmamare/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Root : Entity<Root> {
        struct Quantum {
            RGB ambient;
            float ambient_intensity;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Node_group : Group<Node_group, Root, Node> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Camera_group : Group<Camera_group, Root, Camera> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Light_group : Group<Light_group, Root, Light> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Interface : Manipulation<Interface, Root> {
        struct Actions : BaseActions {
            static auto createScene(Writing) -> Root::Id;
            static auto createCamera(Writing, Root::Id, Locator, Camera::Quantum) -> Camera::Id;
            static auto createLight(Writing, Root::Id, Locator, Light::Quantum) -> Light::Id;
            static auto createPrimitiveActor(Writing, Root::Id, Locator, PrimitiveActor::Quantum) -> PrimitiveActor::Id;
        };
    };

}
