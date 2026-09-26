#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/viewInput.q1.h>

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

            node.pose.position.x += delta.x;
            node.pose.position.y += delta.y;
        }

        void apply_mouse_drag(scene::Node::Quantum& node, index2 delta_mouse) {
            if (delta_mouse.x == 0 && delta_mouse.y == 0) {
                return;
            }
            node.pose.position.x -= static_cast<float>(delta_mouse.x) * k_pan_pixels_to_world;
            node.pose.position.y += static_cast<float>(delta_mouse.y) * k_pan_pixels_to_world;
        }

        auto button_down(const vector<bool>& buttons, int button) -> bool {
            return static_cast<std::size_t>(button) < buttons.size() && buttons[static_cast<std::size_t>(button)];
        }

        void drive(Writing context, Camera2d::Id self, seconds delta_sec) {
            const auto& mail = with<system::ViewInput>::get(context, with<Camera2d>::get(context, self).input);
            if (not mail.engaged)
                return;
            auto node = with<scene::Node>::modify(context, self);

            apply_arrow_move(*node, mail.keys, delta_sec);

            if (button_down(mail.buttons, GLFW_MOUSE_BUTTON_RIGHT))
                apply_mouse_drag(*node, mail.mouseShift);
        }

    } // namespace

    auto Camera2d::Actions::create(Writing context, scene::Camera::Id anchor, system::ViewInput::Id input) -> Id {
        with<Camera2d>::extend(context, anchor, Camera2d::Quantum{.input = input});
        return anchor;
    }

    void Camera2d::Actions::tick(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        // ids first: drive writes into the same session
        std::vector<Id> cameras;
        for (const auto entry : context->aspect<Camera2d>().items())
            cameras.push_back(entry.id);
        for (const auto camera : cameras)
            drive(context, camera, dt);
    }

    auto doctrine::camera2d() -> Schema {
        return ask::schema::aspect<Camera2d>();
    }

}
