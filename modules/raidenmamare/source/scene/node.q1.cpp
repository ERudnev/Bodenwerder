#include <rmmr/scene/node.q1.h>

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        // HPB degrees: heading(Y), pitch(X), bank(Z) — intrinsic YXZ.
        // Not glm::eulerAngles: its yaw() uses asin and only covers [-90°, +90°].

        auto rotation_from_hpb(HPB hpb) -> quat {
            const vec3 radians = glm::radians(hpb);
            const quat heading = glm::angleAxis(radians.x, vec3{0.0f, 1.0f, 0.0f});
            const quat pitch = glm::angleAxis(radians.y, vec3{1.0f, 0.0f, 0.0f});
            const quat bank = glm::angleAxis(radians.z, vec3{0.0f, 0.0f, 1.0f});
            return glm::normalize(heading * pitch * bank);
        }

        auto make_transform(const Node::Quantum& quantum) -> mat4 {
            const auto translate = glm::translate(mat4{1.0f}, quantum.position);
            const auto rotation = glm::mat4_cast(glm::normalize(quantum.rotation));
            return translate * rotation;
        }

        auto heading_pitch_bank_from_rotation(quat rotation) -> vec3 {
            const glm::mat3 matrix = glm::mat3_cast(glm::normalize(rotation));
            const float heading = std::atan2(matrix[2][0], matrix[2][2]);
            const float pitch_cos = std::sqrt(matrix[0][1] * matrix[0][1] + matrix[1][1] * matrix[1][1]);
            const float pitch = std::atan2(-matrix[2][1], pitch_cos);
            const float sin_heading = std::sin(heading);
            const float cos_heading = std::cos(heading);
            const float bank = std::atan2(
                sin_heading * matrix[1][2] - cos_heading * matrix[1][0],
                cos_heading * matrix[0][0] - sin_heading * matrix[0][2]);
            return vec3{heading, pitch, bank};
        }

    } // namespace

    auto Node::Actions::create(Writing context, Locator locator) -> Id {
        return Node::BaseActions::create(context, Node::Quantum{
            .position = locator.pos,
            .rotation = rotation_from_hpb(locator.euler),
        });
    }

    auto Node::Actions::transform(Reading context, Id id) -> mat4 {
        return make_transform(with<Node>::get(context, id));
    }

    auto Node::Actions::hpb(Reading context, Id id) -> HPB {
        return glm::degrees(heading_pitch_bank_from_rotation(with<Node>::get(context, id).rotation));
    }

    void Node::Actions::hpb(Writing context, Id id, HPB value) {
        with<Node>::modify(context, id)->rotation = rotation_from_hpb(value);
    }

}
