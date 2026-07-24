#include <rmmr/scene/camera.q1.h>

#include <base/logging.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        constexpr float k_z_near = 0.1f;
        constexpr float k_z_far = 100.0f;

        auto projection_matrix(const Camera::Quantum& quantum, float aspect_ratio) -> mat4 {
            if (quantum.mode == Camera::Mode::perspective)
                return glm::perspective(quantum.fov_y, aspect_ratio, quantum.z_near, quantum.z_far);
            if (quantum.mode == Camera::Mode::orthographic) {
                const float half_w = 0.5f * static_cast<float>(quantum.ortho_size.x);
                const float half_h = 0.5f * static_cast<float>(quantum.ortho_size.y);
                return glm::ortho(-half_w, half_w, -half_h, half_h, quantum.z_near, quantum.z_far);
            }
            _INCOMPLETE_;
        }

        auto view_matrix(fqsm::Reading context, Camera::Id id) -> mat4 {
            const mat4 node_transform = Node::Actions::transform(context, id);
            return glm::inverse(node_transform);
        }

    } // namespace

    auto Camera::Actions::create(Writing context, Locator locator, float fov_y) -> Id {
        const auto node = Node::Actions::create(context, locator);
        with<Camera>::extend(context, node, Camera::Quantum{
            .mode = Mode::perspective,
            .z_near = k_z_near,
            .z_far = k_z_far,
            .fov_y = fov_y,
            .ortho_size = index2{0, 0},
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
