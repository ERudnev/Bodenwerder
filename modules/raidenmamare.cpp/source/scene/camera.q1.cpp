#include <Raidenmamare/scene/camera.q1.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene {
    namespace {
        auto projection_matrix(const Camera::Quantum& quantum, float aspect_ratio) -> mat4 {
            return glm::perspective(quantum.fov_y, aspect_ratio, quantum.z_near, quantum.z_far);
        }

        auto view_matrix(with<Camera>::Reading context, Camera::Id id) -> mat4 {
            const mat4 node_transform = with<Node>::transform(context, id);
            return glm::inverse(node_transform);
        }
    }

    auto Camera::Actions::create(Writing context, Locator locator, float fov_y, float z_near, float z_far) -> Id {
        const auto node = with<Node>::create(context, locator);
        with<Camera>::extend(context, node, Quantum{.fov_y = fov_y, .z_near = z_near, .z_far = z_far});
        return node;
    }

    auto Camera::Actions::projection(Reading context, Id id, float aspect_ratio) -> mat4 {
        return projection_matrix(get(context, id), aspect_ratio);
    }

    auto Camera::Actions::view(Reading context, Id id) -> mat4 {
        return view_matrix(context, id);
    }

    auto Camera::Actions::view_projection(Reading context, Id id, float aspect_ratio) -> mat4 {
        const mat4 projection = projection_matrix(get(context, id), aspect_ratio);
        const mat4 view = view_matrix(context, id);
        return projection * view;
    }
}
