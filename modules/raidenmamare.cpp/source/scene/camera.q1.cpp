#include <Raidenmamare/scene/camera.q1.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene {
    struct Camera_private : Camera::Operations {
        static auto projection_matrix(const Quantum& quantum, float aspect_ratio) -> mat4 {
            return glm::perspective(quantum.fov_y, aspect_ratio, quantum.z_near, quantum.z_far);
        }

        static auto view_matrix(Reading world, Camera::Id id) -> mat4 {
            const mat4 node_transform = Node::Operations::transform(world, id);
            return glm::inverse(node_transform);
        }
    };

    auto Camera::Operations::create(Writing commit, Pos position, HPB hpb, float fov_y, float z_near, float z_far) -> Id {
        const auto node = Node::Operations::create_posHpb(commit, position, hpb);
        ops::particle::create<Camera>(
            commit,
            node,
            Camera::Quantum{
                .fov_y = fov_y,
                .z_near = z_near,
                .z_far = z_far,
            }
        );
        return node;
    }

    auto Camera::Operations::projection(Reading world, Id id, float aspect_ratio) -> mat4 {
        const auto& q = ops::particle::get<Camera>(world, id);
        return Camera_private::projection_matrix(q, aspect_ratio);
    }

    auto Camera::Operations::view(Reading world, Id id) -> mat4 {
        return Camera_private::view_matrix(world, id);
    }

    auto Camera::Operations::view_projection(Reading world, Id id, float aspect_ratio) -> mat4 {
        const auto& q = ops::particle::get<Camera>(world, id);
        const mat4 projection = Camera_private::projection_matrix(q, aspect_ratio);
        const mat4 view = Camera_private::view_matrix(world, id);
        return projection * view;
    }

    const Invariants Camera::invariants{
        .structural = {},
        .logical = {},
    };
}

