#include <rmmr/scene/root.q1.h>

#include <base/logging.h>

#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        constexpr float k_camera_z_near = 0.1f;
        constexpr float k_camera_z_far = 100.0f;

        auto node_quantum_from_locator(Locator locator) -> Node::Quantum {
            const vec3 radians = glm::radians(locator.euler);
            const quat heading = glm::angleAxis(radians.x, vec3{0.0f, 1.0f, 0.0f});
            const quat pitch = glm::angleAxis(radians.y, vec3{1.0f, 0.0f, 0.0f});
            const quat bank = glm::angleAxis(radians.z, vec3{0.0f, 0.0f, 1.0f});
            return Node::Quantum{
                .position = locator.pos,
                .rotation = glm::normalize(heading * pitch * bank),
            };
        }

        auto attach_camera(Writing context, Root::Id root, Locator locator, Camera::Quantum camera) -> Camera::Id {
            const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
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

    auto Interface::createCamera(Writing context, Root::Id root, Locator locator, float fov_x) -> Camera::Id {
        return attach_camera(context, root, locator, Camera::Quantum{
            .mode = Camera::Mode::perspective,
            .z_near = k_camera_z_near,
            .z_far = k_camera_z_far,
            .fov_x = fov_x,
            .ortho_size = index2{0, 0},
        });
    }

    auto Interface::createLight(Writing context, Root::Id root, Locator locator, Light::Quantum lightQuantum) -> Light::Id {
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<Light_group>::addElement(context, root, node, std::move(lightQuantum));
        return node;
    }

    auto Interface::createSimpleActor(Writing context, Root::Id root, Locator locator, actor::Simple::Quantum actorQuantum) -> actor::Simple::Id {
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<actor::Simple>::extend(context, node, std::move(actorQuantum));
        return node;
    }

    auto Interface::createGrid(Writing context, Root::Id root, Locator locator, Grid::Quantum gridQuantum) -> Grid::Id {
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<Grid>::extend(context, node, std::move(gridQuantum));
        return node;
    }

    void Interface::render(Reading context, Root::Id root, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& node_group = with<Node_group>::get(context, root);
        for (const auto node : node_group) {
            if (with<actor::Simple>::exists(context, node)) {
                actor::Simple::Actions::submit(context, node, device, where);
                continue;
            }
            if (with<actor::Sprite>::exists(context, node)) {
                actor::Sprite::Actions::submit(context, node, device, where);
                continue;
            }
            if (with<Grid>::exists(context, node)) {
                Grid::Actions::submit(context, node, device, where);
            }
        }
    }

    auto Flat2d::Actions::createCamera(Writing context, Id root, Locator locator) -> Camera::Id {
        if (not with<Flat2d>::exists(context, root))
            return context.refuse("scene::Flat2d::createCamera: Flat2d is not installed on this root");
        const auto& flat = with<Flat2d>::get(context, root);
        return attach_camera(context, root, locator, Camera::Quantum{
            .mode = Camera::Mode::orthographic,
            .z_near = k_camera_z_near,
            .z_far = k_camera_z_far,
            .fov_x = 0.0f,
            .ortho_size = flat.size,
        });
    }

    auto Flat2d::Actions::createSpriteActor(Writing context, Id root, Locator locator, actor::Sprite::Quantum sprite) -> actor::Sprite::Id {
        if (not with<Flat2d>::exists(context, root))
            return context.refuse("scene::Flat2d::createSpriteActor: Flat2d is not installed on this root");
        const auto node = with<Node_group>::addElement(context, root, node_quantum_from_locator(locator));
        with<actor::Sprite>::extend(context, node, std::move(sprite));
        return node;
    }

}
