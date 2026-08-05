#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>

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
        static auto createScene(Writing) -> Root::Id;
        static auto createCamera(Writing, Root::Id, Pose, float fov_x) -> Camera::Id;
        static auto createLight(Writing, Root::Id, Pose, Light::Quantum) -> Light::Id;
        static auto createSimpleActor(Writing, Root::Id, Pose, actor::Simple::Quantum) -> actor::Simple::Id;
        static auto createMeshActor(Writing, Root::Id, Pose, actor::Mesh::Quantum) -> actor::Mesh::Id;
        static auto createGrid(Writing, Root::Id, Pose, Grid::Quantum) -> Grid::Id;
        static void render(Reading, Root::Id, system::Device::Id, renderer::CommandBuffer& where);
    };

    // Ortho/2D layer on a scene root — sprite actors created here.
    // createCamera: look along -Z; orthographic extent from Flat2d.size.
    struct Flat2d : Feature<Flat2d, Root> {
        struct Quantum {
            index2 size;
        };
        struct Actions : BaseActions {
            static auto createCamera(Writing, Id, Pose) -> Camera::Id;
            static auto createSpriteActor(Writing, Id, Pose, actor::Sprite::Quantum) -> actor::Sprite::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
