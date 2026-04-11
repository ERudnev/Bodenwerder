#include <Raidenmamare/scene/node.q1.h>

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

        static auto rotation_from_heading_pitch_bank(vec3 heading_pitch_bank) -> quat {
            const glm::vec3 glm_euler{
                heading_pitch_bank.y,
                heading_pitch_bank.x,
                heading_pitch_bank.z,
            };
            return glm::normalize(quat{glm_euler});
        }
    };

    auto Node::Operations::transform(Reading world, Id id) -> mat4 {
        return Node_private::make_transform(ops::particle::get<Node>(world, id));
    }

    auto Node::Operations::euler(Reading world, Id id) -> vec3 {
        return Node_private::heading_pitch_bank_from_rotation(ops::particle::get<Node>(world, id).rotation);
    }

    auto Node::Operations::euler(Writing commit, Id id, vec3 heading_pitch_bank) -> void {
        ops::particle::modifier<Node>(commit, id)->rotation =
            Node_private::rotation_from_heading_pitch_bank(heading_pitch_bank);
    }

    const Invariants Node::invariants{
        .structural = {},
        .logical = {},
    };
}
