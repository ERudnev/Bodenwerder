#include <rmmr/controller/camera.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/window.q1.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

namespace rmmr::controller {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        constexpr float k_mouse_sens_deg_per_pixel = 0.12f;
        constexpr float k_mouse_yaw_scale_x = -1.0f;
        constexpr float k_pitch_min_deg = -89.0f;
        constexpr float k_pitch_max_deg = 89.0f;
        constexpr float k_move_units_per_sec = 3.0f;

        const glm::vec3 k_world_up{0.0f, 1.0f, 0.0f};

        auto orientation_from_forward(glm::vec3 forward) -> glm::quat {
            forward = glm::normalize(forward);
            const glm::vec3 right = glm::normalize(glm::cross(k_world_up, forward));
            const glm::vec3 up = glm::cross(forward, right);
            const glm::mat3 basis(right, up, -forward);
            return glm::normalize(glm::quat_cast(basis));
        }

        void clamp_pitch(glm::quat& rotation) {
            glm::vec3 forward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            const float min_sin = std::sin(glm::radians(k_pitch_min_deg));
            const float max_sin = std::sin(glm::radians(k_pitch_max_deg));
            const float clamped_y = std::clamp(forward.y, min_sin, max_sin);
            if (std::abs(forward.y - clamped_y) <= 1e-6f) {
                return;
            }

            forward.y = clamped_y;
            const float xz_len = std::sqrt(forward.x * forward.x + forward.z * forward.z);
            if (xz_len <= 1e-6f) {
                forward = glm::vec3{0.0f, clamped_y > 0.0f ? 1.0f : -1.0f, 0.0f};
            } else {
                const float scale = std::sqrt(1.0f - clamped_y * clamped_y) / xz_len;
                forward.x *= scale;
                forward.z *= scale;
            }

            rotation = orientation_from_forward(forward);
        }

        void apply_mouse_look(glm::quat& rotation, index2 delta_mouse) {
            if (delta_mouse.x == 0 && delta_mouse.y == 0) {
                return;
            }

            const float sens_rad = glm::radians(k_mouse_sens_deg_per_pixel);
            const float yaw = k_mouse_yaw_scale_x * static_cast<float>(delta_mouse.x) * sens_rad;
            const float pitch = -static_cast<float>(delta_mouse.y) * sens_rad;

            rotation = glm::normalize(glm::angleAxis(yaw, k_world_up) * rotation);

            const glm::vec3 right = glm::normalize(rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            rotation = glm::normalize(glm::angleAxis(pitch, right) * rotation);

            clamp_pitch(rotation);
        }

        void apply_arrow_move(scene::Node::Quantum& node, glm::quat rotation, const vector<bool>& keys, seconds delta_sec) {
            if (delta_sec <= 0.0) {
                return;
            }

            const auto key_down = [&keys](int key) -> bool {
                return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
            };

            rotation = glm::normalize(rotation);
            const glm::vec3 forward_cam = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            const glm::vec3 up_cam = glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec3 forward_xz = forward_cam;
            forward_xz.y = 0.0f;
            if (glm::dot(forward_xz, forward_xz) < 1e-10f) {
                forward_xz = glm::vec3(0.0f, 0.0f, -1.0f);
            } else {
                forward_xz = glm::normalize(forward_xz);
            }

            const glm::vec3 right_xz = glm::normalize(glm::cross(forward_xz, k_world_up));
            const float step = k_move_units_per_sec * static_cast<float>(delta_sec);
            glm::vec3 delta{0.0f};

            if (key_down(GLFW_KEY_UP)) delta += forward_cam * step;
            if (key_down(GLFW_KEY_DOWN)) delta -= forward_cam * step;
            if (key_down(GLFW_KEY_LEFT)) delta -= right_xz * step;
            if (key_down(GLFW_KEY_RIGHT)) delta += right_xz * step;
            if (key_down(GLFW_KEY_PAGE_UP)) delta += up_cam * step;
            if (key_down(GLFW_KEY_PAGE_DOWN)) delta -= up_cam * step;

            if (glm::dot(delta, delta) <= 0.0f) {
                return;
            }

            node.position.x += delta.x;
            node.position.y += delta.y;
            node.position.z += delta.z;
        }

    } // namespace

    auto Camera::Actions::create(Writing context, scene::Camera::Id anchor) -> Id {
        with<Camera>::extend(context, anchor, Camera::Quantum{});
        return anchor;
    }

    void Camera::Actions::update(Writing context, Id self, system::Window::Id window) {
        const auto& input = with<system::Window>::get(context, window);
        const auto& device_quantum = with<system::Device>::get(context, window);
        GLFWwindow* const handle = device_quantum.handle;
        if (not handle) {
            return;
        }

        auto node = with<scene::Node>::modify(context, self);
        glm::quat rotation = glm::normalize(node->rotation);

        apply_arrow_move(*node, rotation, input.current.keys, with<system::Window>::dt(context, window));

        if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            apply_mouse_look(rotation, with<system::Window>::mouseShift(context, window));
            node->rotation = rotation;
        }
    }

}
