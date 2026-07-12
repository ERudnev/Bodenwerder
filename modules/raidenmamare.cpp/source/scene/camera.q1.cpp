#include <rmmr/scene/camera.q1.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        auto projection_matrix(const Camera::Quantum& quantum, float aspect_ratio) -> mat4 {
            return glm::perspective(quantum.fov_y, aspect_ratio, quantum.z_near, quantum.z_far);
        }

        auto view_matrix(fqsm::Reading context, Camera::Id id) -> mat4 {
            const mat4 node_transform = Node::Actions::transform(context, id);
            return glm::inverse(node_transform);
        }

    } // namespace

    auto Camera::Actions::create(Writing context, Locator locator, float fov_y, float z_near, float z_far) -> Id {
        const auto node = Node::Actions::create(context, locator);
        with<Camera>::extend(context, node, Camera::Quantum{
            .fov_y = fov_y,
            .z_near = z_near,
            .z_far = z_far,
        });
        return node;
    }

    auto Camera::Actions::projection(Reading context, Id id, float aspect_ratio) -> mat4 {
        return projection_matrix(with<Camera>::get(context, id), aspect_ratio);
    }

    auto Camera::Actions::view(Reading context, Id id) -> mat4 {
        return view_matrix(context, id);
    }

    auto Camera::Actions::view_projection(Reading context, Id id, float aspect_ratio) -> mat4 {
        const auto& quantum = with<Camera>::get(context, id);
        return projection_matrix(quantum, aspect_ratio) * view_matrix(context, id);
    }

}
