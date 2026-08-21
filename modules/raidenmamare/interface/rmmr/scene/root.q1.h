#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
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
            struct Bloom {
                float radius;
                float intensity;
            };
            RGB ambient;
            float ambient_intensity;
            Bloom bloom;
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
        using Meshes = resource::meshpack::Asset;

        static auto createScene(Writing) -> Root::Id;
        static auto createCamera(Writing, Root::Id, Pose, float fov_x) -> Camera::Id;
        static auto createLight(Writing, Root::Id, Pose, Light::Quantum) -> Light::Id;
        static auto createMeshActor(Writing, Root::Id, Pose, actor::Mesh::Quantum, actor::MeshState::Quantum) -> actor::Mesh::Id;
        static auto createMeshActor(Writing, Root::Id, Pose, Meshes::Resolved) -> actor::Mesh::Id;
        static auto createMeshActor(Writing, Root::Id, Pose, Meshes::Resolved, actor::MeshState::Quantum) -> actor::Mesh::Id;
        static auto createMeshActor(Writing, Root::Id, Pose, Meshes::Id, string entry) -> actor::Mesh::Id;
        static auto createMeshActor(Writing, Root::Id, Pose, Meshes::Id, string entry, actor::MeshState::Quantum) -> actor::Mesh::Id;
        static auto createGrid(Writing, Root::Id, system::Device::Id, Pose, Grid::Quantum) -> Grid::Id;
        static void render(Reading, Root::Id, system::Device::Id, renderer::CommandBuffer& where);
    };

    // Ortho/2D layer on a scene root — sprite actors created here.
    // createCamera: look along -Z; orthographic extent from Flat2d.size.
    struct Flat2d : Feature<Flat2d, Root> {
        struct Quantum {
            index2 size;
            system::Device::Id device;
        };
        struct Actions : BaseActions {
            static auto createCamera(Writing, Id, Pose) -> Camera::Id;
            static auto createSpriteActor(Writing, Id, Pose, actor::Sprite::Quantum) -> actor::Sprite::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
