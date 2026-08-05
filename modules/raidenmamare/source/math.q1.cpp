#include <rmmr/math.q1.h>

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rmmr {

    namespace {

        auto rotation_from_hpb(HPB hpb) -> quat {
            const vec3 radians = glm::radians(hpb);
            const quat heading = glm::angleAxis(radians.x, vec3{0.0f, 1.0f, 0.0f});
            const quat pitch = glm::angleAxis(radians.y, vec3{1.0f, 0.0f, 0.0f});
            const quat bank = glm::angleAxis(radians.z, vec3{0.0f, 0.0f, 1.0f});
            return glm::normalize(heading * pitch * bank);
        }

        auto heading_pitch_bank_from_rotation(quat rotation) -> vec3 {
            const glm::mat3 matrix = glm::mat3_cast(glm::normalize(rotation));
            const float heading = std::atan2(matrix[2][0], matrix[2][2]);
            const float pitch_cos = std::sqrt(matrix[0][1] * matrix[0][1] + matrix[1][1] * matrix[1][1]);
            const float pitch = std::atan2(-matrix[2][1], pitch_cos);
            const float sin_heading = std::sin(heading);
            const float cos_heading = std::cos(heading);
            const float bank = std::atan2(sin_heading * matrix[1][2] - cos_heading * matrix[1][0], cos_heading * matrix[0][0] - sin_heading * matrix[0][2]);
            return vec3{heading, pitch, bank};
        }

    } // namespace

    auto Pose::hpb() const -> HPB { return glm::degrees(heading_pitch_bank_from_rotation(rotation)); }
    void Pose::hpb(HPB value) { rotation = rotation_from_hpb(value); }

    auto Pose::near(const Pose& other) const -> bool {
        constexpr float k_pos_eps = 1e-4f;
        constexpr float k_rot_eps = 1e-5f;
        return glm::distance(position, other.position) <= k_pos_eps and std::abs(glm::dot(rotation, other.rotation)) >= 1.0f - k_rot_eps;
    }

    auto Pose::from(Pos position, HPB hpb) -> Pose {
        Pose pose{.position = position};
        pose.hpb(hpb);
        return pose;
    }

}
