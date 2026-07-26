#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

namespace rmmr::controller {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        constexpr float k_pan_move_units_per_sec = 480.0f;
        constexpr float k_pan_pixels_to_world = 1.0f;

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        void apply_arrow_move(scene::Node::Quantum& node, const vector<bool>& keys, seconds delta_sec) {
            if (delta_sec <= 0.0) {
                return;
            }

            const float step = k_pan_move_units_per_sec * static_cast<float>(delta_sec);
            glm::vec3 delta{0.0f};
            if (key_down(keys, GLFW_KEY_UP)) delta.y += step;
            if (key_down(keys, GLFW_KEY_DOWN)) delta.y -= step;
            if (key_down(keys, GLFW_KEY_LEFT)) delta.x -= step;
            if (key_down(keys, GLFW_KEY_RIGHT)) delta.x += step;

            if (glm::dot(delta, delta) <= 0.0f) {
                return;
            }

            node.position.x += delta.x;
            node.position.y += delta.y;
        }

        void apply_mouse_drag(scene::Node::Quantum& node, index2 delta_mouse) {
            if (delta_mouse.x == 0 && delta_mouse.y == 0) {
                return;
            }
            node.position.x -= static_cast<float>(delta_mouse.x) * k_pan_pixels_to_world;
            node.position.y += static_cast<float>(delta_mouse.y) * k_pan_pixels_to_world;
        }

        void drive(Writing context, Camera2d::Id self, system::Window::Id window, GLFWwindow* handle, seconds delta_sec) {
            const auto& input = with<system::Window>::get(context, window);
            auto node = with<scene::Node>::modify(context, self);

            apply_arrow_move(*node, input.current.keys, delta_sec);

            if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                apply_mouse_drag(*node, with<system::Window>::mouseShift(context, window));
            }
        }

    } // namespace

    auto Camera2d::Actions::create(Writing context, scene::Camera::Id anchor) -> Id {
        with<Camera2d>::extend(context, anchor, Camera2d::Quantum{});
        return anchor;
    }

    struct Camera2d::Internals : Camera2d::DefaultInternals {
        static void update(Reacting context) {
            Writing writing = context;
            for (const auto& change : context.changes<system::Clock>().updated()) {
                const int64 dt_us = change.now.absolute - change.old.absolute;
                if (dt_us <= 0) {
                    continue;
                }
                const seconds delta_sec = static_cast<seconds>(dt_us) / 1'000'000.0;

                for (const auto entry : writing->aspect<system::Window>().items()) {
                    const auto& device = with<system::Device>::get(writing, entry.id);
                    if (not device.handle) {
                        continue;
                    }
                    for (const auto [id, _] : writing->aspect<Camera2d>().items()) {
                        drive(writing, id, entry.id, device.handle, delta_sec);
                    }
                }
            }
        }
    };

    auto Camera2d::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Camera2d, system::Clock>(&Camera2d::Internals::update),
        };
    }

}
