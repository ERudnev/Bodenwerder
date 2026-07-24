#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/scene/node.q1.h>
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
            // Drag content with the cursor: mouse right → camera left.
            node.position.x -= static_cast<float>(delta_mouse.x) * k_pan_pixels_to_world;
            node.position.y += static_cast<float>(delta_mouse.y) * k_pan_pixels_to_world;
        }

        void drive(Writing context, Camera2d::Id self, system::Window::Id window, GLFWwindow* handle) {
            const auto& input = with<system::Window>::get(context, window);
            auto node = with<scene::Node>::modify(context, self);

            apply_arrow_move(*node, input.current.keys, with<system::Window>::dt(context, window));

            if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
                or glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            {
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
            for (const auto change : context.changes<system::Window>().addedOrUpdated()) {
                if (not with<system::Window>::exists(writing, change.id)) {
                    continue;
                }
                const auto& device = with<system::Device>::get(writing, change.id);
                if (not device.handle) {
                    continue;
                }
                for (const auto [id, _] : writing->aspect<Camera2d>().items()) {
                    drive(writing, id, change.id, device.handle);
                }
            }
        }
    };

    auto Camera2d::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Camera2d, system::Window>(&Camera2d::Internals::update),
        };
    }

}
