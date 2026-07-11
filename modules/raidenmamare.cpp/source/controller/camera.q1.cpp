#include <Raidenmamare/controller/camera.q1.h>
#include <Raidenmamare/system/window.q1.h>

#include <GLFW/glfw3.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

#include <algorithm>

namespace rmmr::controller {

    using namespace fqsm::api;

    namespace {

        constexpr float k_mouse_sens_deg_per_pixel = 0.12f;
        constexpr float k_mouse_yaw_scale_x = -1.0f;
        constexpr float k_pitch_min_deg = -89.0f;
        constexpr float k_pitch_max_deg = 89.0f;
        constexpr float k_move_units_per_sec = 3.0f;

        const glm::vec3 k_world_up{0.0f, 1.0f, 0.0f};

        auto fps_orientation_rh(float yaw_rad, float pitch_rad) -> glm::quat {
            const glm::quat q_yaw = glm::angleAxis(yaw_rad, k_world_up);
            const glm::vec3 right_axis = glm::normalize(q_yaw * glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::quat q_pitch = glm::angleAxis(pitch_rad, right_axis);
            return glm::normalize(q_pitch * q_yaw);
        }

        void yaw_pitch_from_node_rotation(glm::quat rotation, float& yaw_out, float& pitch_out) {
            rotation = glm::normalize(rotation);
            const glm::vec3 forward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            yaw_out = std::atan2(forward.x, -forward.z);
            pitch_out = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
        }

        void apply_arrow_move(Writing context, scene::Camera::Id camera, float yaw_rad, float pitch_rad, const vector<bool>& keys, seconds delta_sec) {
            if (delta_sec <= 0.0) {
                return;
            }

            const auto key_down = [&keys](int key) -> bool {
                return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
            };

            const glm::quat q_yaw = glm::angleAxis(yaw_rad, k_world_up);
            glm::vec3 forward_xz = q_yaw * glm::vec3(0.0f, 0.0f, -1.0f);
            forward_xz.y = 0.0f;
            if (glm::dot(forward_xz, forward_xz) < 1e-10f) {
                forward_xz = glm::vec3(0.0f, 0.0f, -1.0f);
            } else {
                forward_xz = glm::normalize(forward_xz);
            }

            const glm::vec3 right_xz = glm::normalize(glm::cross(forward_xz, k_world_up));
            const glm::quat orient = fps_orientation_rh(yaw_rad, pitch_rad);
            const glm::vec3 forward_cam = glm::normalize(orient * glm::vec3(0.0f, 0.0f, -1.0f));
            const float step = k_move_units_per_sec * static_cast<float>(delta_sec);
            glm::vec3 delta{0.0f};

            if (key_down(GLFW_KEY_UP)) delta += forward_cam * step;
            if (key_down(GLFW_KEY_DOWN)) delta -= forward_cam * step;
            if (key_down(GLFW_KEY_LEFT)) delta -= right_xz * step;
            if (key_down(GLFW_KEY_RIGHT)) delta += right_xz * step;

            if (glm::dot(delta, delta) <= 0.0f) {
                return;
            }

            auto node = with<scene::Node>::modify(context, camera);
            node->position.x += delta.x;
            node->position.y += delta.y;
            node->position.z += delta.z;
        }

    } // namespace

    auto Camera::Actions::create(Writing context, scene::Camera::Id anchor) -> Id {
        with<Camera>::extend(context, anchor, Camera::Quantum{
            .yaw_rad = 0.0f,
            .pitch_rad = 0.0f,
            .fps_initialized = false,
        });
        return anchor;
    }

    void Camera::Actions::update(Writing context, Id self, system::Window::Id window) {
        const auto& input = with<system::Window>::get(context, window);
        const auto& device_quantum = with<system::Device>::get(context, window);
        GLFWwindow* const handle = device_quantum.handle;
        if (not handle) {
            return;
        }

        auto state = with<Camera>::modify(context, self);

        float yaw_rad = state->yaw_rad;
        float pitch_rad = state->pitch_rad;
        if (not state->fps_initialized) {
            yaw_pitch_from_node_rotation(with<scene::Node>::get(context, self).rotation, yaw_rad, pitch_rad);
        }

        apply_arrow_move(context, self, yaw_rad, pitch_rad, input.current.keys, with<system::Window>::dt(context, window));

        if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            const index2 delta_mouse = with<system::Window>::mouseShift(context, window);
            const float sens_rad = glm::radians(k_mouse_sens_deg_per_pixel);
            yaw_rad += k_mouse_yaw_scale_x * static_cast<float>(delta_mouse.x) * sens_rad;
            pitch_rad -= static_cast<float>(delta_mouse.y) * sens_rad;
            pitch_rad = std::clamp(pitch_rad, glm::radians(k_pitch_min_deg), glm::radians(k_pitch_max_deg));
            with<scene::Node>::modify(context, self)->rotation = fps_orientation_rh(yaw_rad, pitch_rad);
        }

        state->yaw_rad = yaw_rad;
        state->pitch_rad = pitch_rad;
        state->fps_initialized = true;
    }

}
