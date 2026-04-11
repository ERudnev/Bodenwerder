#include <Raidenmamare/scene/node.q1.h>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {
    struct Node_private : Node::Operations {
        static auto make_transform(const Quantum& quantum) -> mat4 {
            const auto translate = glm::translate(mat4{1.0f}, quantum.position);
            const auto rotation = glm::mat4_cast(glm::normalize(quantum.rotation));
            return translate * rotation;
        }

        // GLM eulerAngles: pitch=x, yaw=y, roll=z. Facade vec3: x=heading (yaw), y=pitch, z=bank (roll).
        static auto heading_pitch_bank_from_rotation(quat rotation) -> vec3 {
            const glm::vec3 euler = glm::eulerAngles(glm::normalize(rotation));
            return vec3{euler.y, euler.x, euler.z};
        }

        static auto rotation_from_hpb(HPB hpb) -> quat {
            const vec3 radians = glm::radians(hpb);
            const glm::vec3 glm_euler{
                radians.y,
                radians.x,
                radians.z,
            };
            return glm::normalize(quat{glm_euler});
        }
    };

    auto Node::Operations::create_posHpb(Writing commit, Pos position, HPB hpb) -> Id {
        return ops::particle::create<Node>(
            commit,
            Quantum{
                .position = position,
                .rotation = Node_private::rotation_from_hpb(hpb),
            });
    }

    auto Node::Operations::transform(Reading world, Id id) -> mat4 {
        return Node_private::make_transform(ops::particle::get<Node>(world, id));
    }

    auto Node::Operations::hpb(Reading world, Id id) -> HPB {
        const vec3 radians = Node_private::heading_pitch_bank_from_rotation(ops::particle::get<Node>(world, id).rotation);
        return glm::degrees(radians);
    }

    auto Node::Operations::hpb(Writing commit, Id id, HPB hpb) -> void {
        ops::particle::modifier<Node>(commit, id)->rotation =
            Node_private::rotation_from_hpb(hpb);
    }

    const Invariants Node::invariants{
        .structural = {},
        .logical = {},
    };
}
