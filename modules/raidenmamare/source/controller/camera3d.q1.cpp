#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>

#include <GLFW/glfw3.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

namespace rmmr::controller {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        constexpr float k_mouse_sens_deg_per_pixel = 0.12f;
        constexpr float k_mouse_yaw_scale_x = -1.0f;
        constexpr float k_move_units_per_sec = 30.0f;
        constexpr float k_roll_deg_per_sec = 90.0f;

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        auto button_down(const system::Window::InputState& input, int button) -> bool {
            return static_cast<std::size_t>(button) < input.buttons.size() && input.buttons[static_cast<std::size_t>(button)];
        }

        auto keyBoost(const vector<bool>& keys) -> float {
            return (key_down(keys, GLFW_KEY_LEFT_SHIFT) || key_down(keys, GLFW_KEY_RIGHT_SHIFT)) ? 10.0f : 1.0f;
        }

        void applyMouseLook(glm::quat& rotation, index2 deltaMouse) {
            if (deltaMouse.x == 0 && deltaMouse.y == 0) return;
            const float sensRad = glm::radians(k_mouse_sens_deg_per_pixel);
            const float yaw = k_mouse_yaw_scale_x * static_cast<float>(deltaMouse.x) * sensRad;
            const float pitch = -static_cast<float>(deltaMouse.y) * sensRad;
            rotation = glm::normalize(rotation * glm::angleAxis(pitch, glm::vec3{1.0f, 0.0f, 0.0f}) * glm::angleAxis(yaw, glm::vec3{0.0f, 1.0f, 0.0f}));
        }

        void applyRoll(glm::quat& rotation, const vector<bool>& keys, seconds deltaSec) {
            if (deltaSec <= 0.0) return;
            float roll = 0.0f;
            if (key_down(keys, GLFW_KEY_Q)) roll += 1.0f;
            if (key_down(keys, GLFW_KEY_E)) roll -= 1.0f;
            if (roll == 0.0f) return;
            const float angle = roll * glm::radians(k_roll_deg_per_sec) * keyBoost(keys) * static_cast<float>(deltaSec);
            rotation = glm::normalize(rotation * glm::angleAxis(angle, glm::vec3{0.0f, 0.0f, 1.0f}));
        }

        void applyMove(scene::Node::Quantum& node, glm::quat rotation, const vector<bool>& keys, seconds deltaSec) {
            if (deltaSec <= 0.0) return;
            rotation = glm::normalize(rotation);
            const glm::vec3 forward = glm::normalize(rotation * glm::vec3{0.0f, 0.0f, -1.0f});
            const glm::vec3 right = glm::normalize(rotation * glm::vec3{1.0f, 0.0f, 0.0f});
            const glm::vec3 up = glm::normalize(rotation * glm::vec3{0.0f, 1.0f, 0.0f});
            const float step = k_move_units_per_sec * keyBoost(keys) * static_cast<float>(deltaSec);
            glm::vec3 delta{0.0f};
            if (key_down(keys, GLFW_KEY_W)) delta += forward * step;
            if (key_down(keys, GLFW_KEY_S)) delta -= forward * step;
            if (key_down(keys, GLFW_KEY_A)) delta -= right * step;
            if (key_down(keys, GLFW_KEY_D)) delta += right * step;
            if (key_down(keys, GLFW_KEY_R)) delta += up * step;
            if (key_down(keys, GLFW_KEY_F)) delta -= up * step;
            if (glm::dot(delta, delta) <= 0.0f) return;
            node.pose.position.x += delta.x;
            node.pose.position.y += delta.y;
            node.pose.position.z += delta.z;
        }

        void drive(Writing context, Camera3d::Id self, system::Window::Id window, seconds deltaSec) {
            const auto& input = with<system::Window>::get(context, window);
            auto node = with<scene::Node>::modify(context, self);
            glm::quat rotation = glm::normalize(node->pose.rotation);
            applyRoll(rotation, input.current.keys, deltaSec);
            if (button_down(input.current, GLFW_MOUSE_BUTTON_RIGHT))
                applyMouseLook(rotation, with<system::Window>::mouseShift(context, window));
            node->pose.rotation = rotation;
            applyMove(*node, rotation, input.current.keys, deltaSec);
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
                    for (const auto [id, _] : context.proposal.aspect<Camera3d>().items()) {
                        drive(context, id, entry.id, delta_sec);
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
