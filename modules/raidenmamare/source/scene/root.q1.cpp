#include <rmmr/scene/root.q1.h>

#include <base/logging.h>
#include <rmmr/resources/runtimes.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        constexpr float k_camera_z_near = 1.0f;
        constexpr float k_camera_z_far = 1000.0f;

        auto attach_camera(Writing context, Root::Id root, Pose pose, Camera::Quantum camera) -> Camera::Id {
            const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose, .visible = true});
            with<Camera_group>::addElement(context, root, node, std::move(camera));
            return node;
        }

    } // namespace

    auto Interface::createScene(Writing context) -> Root::Id {
        const auto root = with<Root>::create(context, Root::Quantum{
            .ambient = RGB{0.4f, 0.4f, 0.4f},
            .ambient_intensity = 0.8f,
            .bloom = {.radius = 5.0f, .intensity = 1.0f},
            .gravity = vec3{0.0f, 0.0f, 0.0f},
            .atmosphereDensity = 0.0f,
            .primaryLight = {},
        });

        with<Node_group>::extend(context, root);
        with<Camera_group>::extend(context, root);
        with<Light_group>::extend(context, root);
        with<Family_group>::extend(context, root);

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
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose, .visible = true});
        with<Light_group>::addElement(context, root, node, std::move(lightQuantum));
        auto quantum = with<Root>::modify(context, root);
        if (not quantum->primaryLight)
            quantum->primaryLight = node;
        return node;
    }

    auto Interface::createMeshActor(Writing context, Root::Id root, Pose pose, actor::Mesh::Quantum actorQuantum, actor::MeshState::Quantum stateQuantum) -> actor::Mesh::Id {
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose, .visible = true});
        with<actor::Mesh>::extend(context, node, std::move(actorQuantum));
        with<actor::MeshState>::extend(context, node, std::move(stateQuantum));
        return node;
    }

    auto Interface::createMeshActor(Writing context, Root::Id root, Pose pose, Meshes::Resolved resolved, actor::MeshState::Quantum stateQuantum) -> actor::Mesh::Id {
        auto mesh = with<actor::Mesh>::compose(context, std::move(resolved));
        if (not mesh) return context.refuse("scene::Interface::createMeshActor: mesh composition failed");
        return createMeshActor(context, root, pose, std::move(*mesh), std::move(stateQuantum));
    }

    auto Interface::createMeshActor(Writing context, Root::Id root, Pose pose, Meshes::Resolved resolved) -> actor::Mesh::Id {
        return createMeshActor(context, root, pose, std::move(resolved), with<actor::MeshState>::defaults());
    }

    auto Interface::createMeshActor(Writing context, Root::Id root, Pose pose, Meshes::Id pack, string entry, actor::MeshState::Quantum stateQuantum) -> actor::Mesh::Id {
        auto resolved = with<Meshes>::resolve(context, pack, std::move(entry));
        if (not resolved) return context.refuse("scene::Interface::createMeshActor: meshpack entry resolve failed");
        return createMeshActor(context, root, pose, std::move(*resolved), std::move(stateQuantum));
    }

    auto Interface::createMeshActor(Writing context, Root::Id root, Pose pose, Meshes::Id pack, string entry) -> actor::Mesh::Id {
        return createMeshActor(context, root, pose, pack, std::move(entry), with<actor::MeshState>::defaults());
    }

    auto Interface::createGrid(Writing context, Root::Id root, system::Device::Id, Pose pose, Grid::Quantum gridQuantum) -> Grid::Id {
        auto mesh = with<actor::Mesh>::composeOne(context, gridQuantum.geometry, gridQuantum.material);
        if (not mesh) return context.refuse("scene::Interface::createGrid: mesh composition failed");
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose, .visible = true});
        with<actor::Mesh>::extend(context, node, std::move(*mesh));
        with<actor::MeshState>::extend(context, node, actor::MeshState::Quantum{.albedo = RGB{0.0f, 0.0f, 0.0f}, .scale = vec3{1.0f, 1.0f, 1.0f}, .latticeStep = 1.0f, .patternScale = gridQuantum.patternScale, .opacity = gridQuantum.opacity, .heat = vec2{0.0f, 1.0f}});
        with<Grid>::extend(context, node, std::move(gridQuantum));
        return node;
    }

    auto Interface::createFamily(Writing context, Root::Id root, actor::Family::Quantum familyQuantum) -> actor::Family::Id {
        const auto family = with<Family_group>::addElement(context, root, std::move(familyQuantum));
        with<actor::Replica_group>::extend(context, family);
        return family;
    }

    auto Interface::createFamily(Writing context, Root::Id root, Meshes::Resolved resolved, actor::Family::Layout layout) -> actor::Family::Id {
        auto family = with<actor::Family>::compose(context, std::move(resolved), std::move(layout));
        if (not family) return context.refuse("scene::Interface::createFamily: family composition failed");
        return createFamily(context, root, std::move(*family));
    }

    auto Interface::createFamily(Writing context, Root::Id root, Meshes::Id pack, string entry, actor::Family::Layout layout) -> actor::Family::Id {
        auto resolved = with<Meshes>::resolve(context, pack, std::move(entry));
        if (not resolved) return context.refuse("scene::Interface::createFamily: meshpack entry resolve failed");
        return createFamily(context, root, std::move(*resolved), std::move(layout));
    }

    auto Interface::createReplica(Writing context, Root::Id root, actor::Family::Id family, Pose pose, actor::Replica::Quantum replica) -> actor::Replica::Id {
        if (not with<actor::Family>::exists(context, family))
            return context.refuse("scene::Interface::createReplica: family missing");
        if (not with<actor::Replica_group>::exists(context, family))
            return context.refuse("scene::Interface::createReplica: family flock is not installed");
        if (replica.family != family)
            return context.refuse("scene::Interface::createReplica: replica.family is not this family");
        const auto bytes = with<actor::Family>::get(context, family).layout.instanceBytes;
        if (bytes < 0 or replica.packed.size() != static_cast<std::size_t>(bytes))
            return context.refuse("scene::Interface::createReplica: packed size is not Family.layout.instanceBytes");
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose, .visible = true});
        with<actor::Replica_group>::addElement(context, family, node, std::move(replica));
        return node;
    }

    void Interface::render(Reading context, Root::Id root, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& node_group = with<Node_group>::get(context, root);

        // TODO: refactor this stuff: one complex loop -> many simplier (type-alligned) loops
        for (const auto node : node_group) {
            if (with<actor::Mesh>::exists(context, node)) {
                with<actor::Mesh>::submit(context, node, device, where);
                if (with<actor::Identified>::exists(context, node))
                    with<actor::Identified>::submit(context, node, device, where);
                continue;
            }
        }
        if (with<Family_group>::exists(context, root)) {
            for (const auto family : with<Family_group>::get(context, root)) {
                with<actor::Family>::submit(context, family, device, where);
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
        auto mesh = with<actor::Mesh>::composeOne(context, *global.geometry, sprite.material);
        if (not mesh) return context.refuse("scene::Flat2d::createSpriteActor: mesh composition failed");
        mesh->sprite = runtime->second;
        mesh->spriteIndex = sprite.index;
        const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.pose = pose, .visible = true});
        with<actor::Mesh>::extend(context, node, std::move(*mesh));
        with<actor::MeshState>::extend(context, node, with<actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f} + sprite.tint, sprite.opacity, sprite.scale));
        with<actor::Sprite>::extend(context, node, std::move(sprite));
        return node;
    }

}
