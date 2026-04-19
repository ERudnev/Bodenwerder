#include <Raidenmamare/controller/camera.q1.h>
#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/scene/node.q1.h>

#include <GLFW/glfw3.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

#include <iQSM/api/_gateway_equal.h>
#include <iQSM/helpers/global.h>
#include <iQSM/helpers/particle.h>
#include <iQSM/repository/transactions/sequence.h>

namespace rmmr::controller {

    namespace {
        constexpr float k_mouse_sens_deg_per_pixel = 0.12f;
        constexpr float k_pitch_min_deg = -89.0f;
        constexpr float k_pitch_max_deg = 89.0f;
        constexpr float k_move_units_per_sec = 3.0f;

        static const glm::vec3 k_world_up{0.0f, 1.0f, 0.0f};

        // Right-handed: first yaw around +Y, then pitch around local +X after yaw. View uses local −Z as forward.
        static auto fps_orientation_rh(float yaw_rad, float pitch_rad) -> glm::quat {
            const glm::quat q_yaw = glm::angleAxis(yaw_rad, k_world_up);
            const glm::vec3 right_axis = glm::normalize(q_yaw * glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::quat q_pitch = glm::angleAxis(pitch_rad, right_axis);
            return glm::normalize(q_pitch * q_yaw);
        }

        static void yaw_pitch_from_node_rotation(glm::quat rot, float& yaw_out, float& pitch_out) {
            rot = glm::normalize(rot);
            const glm::vec3 forward = glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
            yaw_out = std::atan2(forward.x, -forward.z);
            pitch_out = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
        }


        static void apply_arrow_move_xz(
            Writing commit,
            scene::Camera::Id node_id,
            float yaw_rad,
            const vector<bool>& keys,
            double delta_sec
        ) {
            if (delta_sec <= 0.0)
                return commit.discard();

            const auto key_down = [&keys](int key) -> bool {
                return static_cast<size_t>(key) < keys.size() && keys[static_cast<size_t>(key)];
            };

            const glm::quat q_yaw = glm::angleAxis(yaw_rad, k_world_up);
            glm::vec3 forward_xz = q_yaw * glm::vec3(0.0f, 0.0f, -1.0f);
            forward_xz.y = 0.0f;
            if (glm::dot(forward_xz, forward_xz) < 1e-10f)
                forward_xz = glm::vec3(0.0f, 0.0f, -1.0f);
            else
                forward_xz = glm::normalize(forward_xz);

            const glm::vec3 right_xz = glm::normalize(glm::cross(forward_xz, k_world_up));

            const float step = k_move_units_per_sec * static_cast<float>(delta_sec);
            glm::vec3 delta{0.0f};

            if (key_down(GLFW_KEY_UP))
                delta += forward_xz * step;
            if (key_down(GLFW_KEY_DOWN))
                delta -= forward_xz * step;
            if (key_down(GLFW_KEY_LEFT))
                delta -= right_xz * step;
            if (key_down(GLFW_KEY_RIGHT))
                delta += right_xz * step;

            if (glm::dot(delta, delta) <= 0.0f)
                return commit.discard();

            auto node_mod = ops::particle::modifier<scene::Node>(commit, node_id);
            node_mod->position.x += delta.x;
            node_mod->position.y += delta.y;
            node_mod->position.z += delta.z;
        }
    }

    auto Camera::Operations::create(Writing permit, scene::Camera::Id anchor) -> Id {
        repo::Sequence transaction(permit);
        ops::particle::create<Dispatcher>(transaction, anchor, Dispatcher::Quantum{});
        ops::particle::create<Camera>(
            transaction,
            anchor,
            Quantum{
                .camera = anchor,
                .previous_mouse = index2{0, 0},
                .has_previous = false,
                .last_step_sec = -1.0,
                .yaw_rad = 0.0f,
                .pitch_rad = 0.0f,
                .fps_initialized = false,
            });
        return anchor;
    }

    void Camera::Operations::update(Writing permit, Id self, seconds nowSec) {
        iqsm::repo::Sequence seq{permit};

        const auto& dispatcher = *ops::global::get<Dispatcher>(seq);
        const auto& state = ops::particle::get<Camera>(seq, self);
        const index2 mouse = dispatcher.mouse;

        const auto device = dispatcher.device;
        if (not device)
            return;
        GLFWwindow* const window = Device::Operations::provide(seq, *device);
        if (!window)
            return;

        float yawRad = state.yaw_rad;
        float pitchRad = state.pitch_rad;
        if (not state.fps_initialized) {
            const glm::quat rotation = ops::particle::get<scene::Node>(seq, state.camera).rotation;
            yaw_pitch_from_node_rotation(rotation, yawRad, pitchRad);
        }

        const double deltaSec = state.last_step_sec >= 0.0 ? nowSec - state.last_step_sec : 0.0;
        apply_arrow_move_xz(seq, state.camera, yawRad, dispatcher.keys, deltaSec);

        bool hasPrevious = false;
        index2 previousMouse = state.previous_mouse;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            hasPrevious = true;
            previousMouse = mouse;
            if (state.has_previous) {
                const index2 deltaMouse{mouse.x - state.previous_mouse.x, mouse.y - state.previous_mouse.y};
                const float sensRad = glm::radians(k_mouse_sens_deg_per_pixel);
                yawRad += static_cast<float>(deltaMouse.x) * sensRad;
                pitchRad -= static_cast<float>(deltaMouse.y) * sensRad;
                pitchRad = std::clamp(pitchRad, glm::radians(k_pitch_min_deg), glm::radians(k_pitch_max_deg));

                auto nodeMod = ops::particle::modifier<scene::Node>(seq, state.camera);
                nodeMod->rotation = fps_orientation_rh(yawRad, pitchRad);
            }
        }


        auto mod = ops::particle::modifier<Camera>(seq, self);
        mod->previous_mouse = previousMouse;
        mod->has_previous = hasPrevious;
        mod->yaw_rad = yawRad;
        mod->pitch_rad = pitchRad;
        mod->fps_initialized = true;
        mod->last_step_sec = nowSec;

    }

    const Invariants Camera::invariants{
        .structural = {},
        .logical = {},
    };
}
