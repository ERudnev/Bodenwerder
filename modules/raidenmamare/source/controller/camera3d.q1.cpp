#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>
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

        void apply_mouse_look(glm::quat& rotation, index2 delta_mouse) {
            if (delta_mouse.x == 0 && delta_mouse.y == 0) return;

            const float sens_rad = glm::radians(k_mouse_sens_deg_per_pixel);
            const float yaw = k_mouse_yaw_scale_x * static_cast<float>(delta_mouse.x) * sens_rad;
            const float pitch_delta = -static_cast<float>(delta_mouse.y) * sens_rad;

            rotation = glm::normalize(glm::angleAxis(yaw, k_world_up) * rotation);

            const glm::vec3 forward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            const float pitch_now = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
            const float pitch = std::clamp(pitch_now + pitch_delta, glm::radians(k_pitch_min_deg), glm::radians(k_pitch_max_deg)) - pitch_now;
            if (std::abs(pitch) <= 1e-8f) return;

            const glm::vec3 right = glm::normalize(rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            rotation = glm::normalize(glm::angleAxis(pitch, right) * rotation);
        }

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        void apply_arrow_move(scene::Node::Quantum& node, glm::quat rotation, const vector<bool>& keys, seconds delta_sec) {
            if (delta_sec <= 0.0) {
                return;
            }

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

            if (key_down(keys, GLFW_KEY_UP)) delta += forward_cam * step;
            if (key_down(keys, GLFW_KEY_DOWN)) delta -= forward_cam * step;
            if (key_down(keys, GLFW_KEY_LEFT)) delta -= right_xz * step;
            if (key_down(keys, GLFW_KEY_RIGHT)) delta += right_xz * step;
            if (key_down(keys, GLFW_KEY_PAGE_UP)) delta += up_cam * step;
            if (key_down(keys, GLFW_KEY_PAGE_DOWN)) delta -= up_cam * step;

            if (glm::dot(delta, delta) <= 0.0f) {
                return;
            }

            node.position.x += delta.x;
            node.position.y += delta.y;
            node.position.z += delta.z;
        }

        void drive(Writing context, Camera3d::Id self, system::Window::Id window, GLFWwindow* handle, seconds delta_sec) {
            const auto& input = with<system::Window>::get(context, window);
            auto node = with<scene::Node>::modify(context, self);
            glm::quat rotation = glm::normalize(node->rotation);

            apply_arrow_move(*node, rotation, input.current.keys, delta_sec);

            if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                apply_mouse_look(rotation, with<system::Window>::mouseShift(context, window));
                node->rotation = rotation;
            }
        }

    } // namespace

    auto Camera3d::Actions::create(Writing context, scene::Camera::Id anchor) -> Id {
        with<Camera3d>::extend(context, anchor, Camera3d::Quantum{});
        return anchor;
    }

    struct Camera3d::Internals : Camera3d::DefaultInternals {
        static void update(Reacting context) {
            for (const auto& change : context.changes<system::Clock>().updated()) {
                const int64 dt_us = change.now.absolute - change.old.absolute;
                if (dt_us <= 0) {
                    continue;
                }
                const seconds delta_sec = static_cast<seconds>(dt_us) / 1'000'000.0;

                for (const auto entry : context.proposal.aspect<system::Window>().items()) {
                    const auto& device = with<system::Device>::get(context, entry.id);
                    if (not device.handle) {
                        continue;
                    }
                    for (const auto [id, _] : context.proposal.aspect<Camera3d>().items()) {
                        drive(context, id, entry.id, device.handle, delta_sec);
                    }
                }
            }
        }
    };

    auto Camera3d::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Camera3d, system::Clock>(&Camera3d::Internals::update),
        };
    }

}
