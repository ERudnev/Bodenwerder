#include <rmmr/scene/root.q1.h>

#include <base/logging.h>
#include <rmmr/resources/runtimes.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        constexpr float k_camera_z_near = 1.0f;
        constexpr float k_camera_z_far = 1000.0f;

        auto attach_camera(Writing context, Root::Id root, Pose pose, Camera::Quantum camera) -> Camera::Id {
            const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose});
            with<Camera_group>::addElement(context, root, node, std::move(camera));
            return node;
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

    auto Interface::createCamera(Writing context, Root::Id root, Pose pose, float fov_x) -> Camera::Id {
        return attach_camera(context, root, pose, Camera::Quantum{
            .mode = Camera::Mode::perspective,
            .z_near = k_camera_z_near,
            .z_far = k_camera_z_far,
            .fov_x = fov_x,
            .ortho_size = index2{0, 0},
        });
    }

    auto Interface::createLight(Writing context, Root::Id root, Pose pose, Light::Quantum lightQuantum) -> Light::Id {
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose});
        with<Light_group>::addElement(context, root, node, std::move(lightQuantum));
        return node;
    }

    auto Interface::createMeshActor(Writing context, Root::Id root, Pose pose, actor::Mesh::Quantum actorQuantum, actor::MeshState::Quantum stateQuantum) -> actor::Mesh::Id {
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose});
        with<actor::Mesh>::extend(context, node, std::move(actorQuantum));
        with<actor::MeshState>::extend(context, node, std::move(stateQuantum));
        return node;
    }

    auto Interface::createGrid(Writing context, Root::Id root, system::Device::Id device, Pose pose, Grid::Quantum gridQuantum) -> Grid::Id {
        auto mesh = actor::Mesh::Actions::composeOne(context, device, gridQuantum.geometry, gridQuantum.material);
        if (not mesh) return context.refuse("scene::Interface::createGrid: mesh composition failed");
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose});
        with<actor::Mesh>::extend(context, node, std::move(*mesh));
        with<actor::MeshState>::extend(context, node, actor::MeshState::Quantum{.albedo = RGB{0.0f, 0.0f, 0.0f}, .scale = vec3{1.0f, 1.0f, 1.0f}, .latticeStep = 1.0f, .patternScale = gridQuantum.patternScale, .opacity = gridQuantum.opacity, .visible = true});
        with<Grid>::extend(context, node, std::move(gridQuantum));
        return node;
    }

    void Interface::render(Reading context, Root::Id root, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& node_group = with<Node_group>::get(context, root);

        // TODO: refactor this stuff: one complex loop -> many simplier (type-alligned) loops
        for (const auto node : node_group) {
            if (with<actor::Mesh>::exists(context, node)) {
                actor::Mesh::Actions::submit(context, node, device, where);
                if (with<actor::Identified>::exists(context, node))
                    actor::Identified::Actions::submit(context, node, device, where);
                continue;
            }
        }
    }

    auto Flat2d::Actions::createCamera(Writing context, Id root, Pose pose) -> Camera::Id {
        if (not with<Flat2d>::exists(context, root))
            return context.refuse("scene::Flat2d::createCamera: Flat2d is not installed on this root");
        const auto& flat = with<Flat2d>::get(context, root);
        return attach_camera(context, root, pose, Camera::Quantum{
            .mode = Camera::Mode::orthographic,
            .z_near = k_camera_z_near,
            .z_far = k_camera_z_far,
            .fov_x = 0.0f,
            .ortho_size = flat.size,
        });
    }

    auto Flat2d::Actions::createSpriteActor(Writing context, Id root, Pose pose, actor::Sprite::Quantum sprite) -> actor::Sprite::Id {
        if (not with<Flat2d>::exists(context, root))
            return context.refuse("scene::Flat2d::createSpriteActor: Flat2d is not installed on this root");
        const auto& flat = with<Flat2d>::get(context, root);
        const auto& global = with<actor::Sprite>::get_global(context);
        if (not global.geometry) return context.refuse("scene::Flat2d::createSpriteActor: shared geometry missing");
        const auto& runtimes = with<resource::Runtimes>::get(context, flat.device);
        const auto runtime = runtimes.sprites_id_mapping.find(sprite.pack);
        if (runtime == runtimes.sprites_id_mapping.end() or not with<resource::sprite::Runtime>::exists(context, runtime->second)) return context.refuse("scene::Flat2d::createSpriteActor: sprite runtime missing");
        auto mesh = actor::Mesh::Actions::composeOne(context, flat.device, *global.geometry, sprite.material);
        if (not mesh) return context.refuse("scene::Flat2d::createSpriteActor: mesh composition failed");
        mesh->sprite = runtime->second;
        mesh->spriteIndex = sprite.index;
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose});
        with<actor::Mesh>::extend(context, node, std::move(*mesh));
        with<actor::MeshState>::extend(context, node, actor::MeshState::Quantum{.albedo = RGB{1.0f, 1.0f, 1.0f} + sprite.tint, .scale = sprite.scale, .latticeStep = 1.0f, .patternScale = 1.0f, .opacity = sprite.opacity, .visible = true});
        with<actor::Sprite>::extend(context, node, std::move(sprite));
        return node;
    }

}
