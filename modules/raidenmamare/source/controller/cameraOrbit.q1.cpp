#include <rmmr/controller/cameraOrbit.q1.h>
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

        constexpr float k_mouse_sens_deg_per_pixel = 0.25f;
        constexpr float k_heading_scale_x = -1.0f;
        constexpr float k_pitch_min_deg = -89.0f;
        constexpr float k_pitch_max_deg = 89.0f;
        constexpr float k_distance_min = 0.5f;
        constexpr float k_distance_max = 500.0f;
        constexpr float k_zoom_wheel_base = 0.85f; // distance *= base^wheel (scroll up → closer)
        constexpr float k_pan_units_per_sec = 24.0f;

        const glm::vec3 k_world_up{0.0f, 1.0f, 0.0f};

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        auto button_down(const system::Window::InputState& input, int button) -> bool {
            return static_cast<std::size_t>(button) < input.buttons.size() && input.buttons[static_cast<std::size_t>(button)];
        }

        auto rotation_from_orbit_hpb(HPB hpb) -> quat {
            hpb.z = 0.0f;
            return Pose::from(Pos{0.0f, 0.0f, 0.0f}, hpb).rotation;
        }

        void apply_pose(scene::Node::Quantum& node, const CameraOrbit::Quantum& orbit) {
            const quat rotation = rotation_from_orbit_hpb(orbit.hpb);
            const vec3 forward = glm::normalize(rotation * vec3{0.0f, 0.0f, -1.0f});
            node.pose.rotation = rotation;
            node.pose.position = orbit.pivot - forward * orbit.distance;
        }

        void drive(Writing context, CameraOrbit::Id self, system::Window::Id window, seconds delta_sec) {
            const auto& input = with<system::Window>::get(context, window);
            auto orbit = with<CameraOrbit>::modify(context, self);

            if (button_down(input.current, GLFW_MOUSE_BUTTON_RIGHT)) {
                const auto shift = with<system::Window>::mouseShift(context, window);
                orbit->hpb.x += k_heading_scale_x * static_cast<float>(shift.x) * k_mouse_sens_deg_per_pixel;
                orbit->hpb.y += -static_cast<float>(shift.y) * k_mouse_sens_deg_per_pixel;
                orbit->hpb.y = std::clamp(orbit->hpb.y, k_pitch_min_deg, k_pitch_max_deg);
                orbit->hpb.z = 0.0f;
            }

            if (std::abs(input.current.wheel) > 1e-6f)
                orbit->distance *= std::pow(k_zoom_wheel_base, input.current.wheel);

            if (delta_sec > 0.0) {
                const quat rotation = rotation_from_orbit_hpb(orbit->hpb);
                const vec3 forward = glm::normalize(rotation * vec3{0.0f, 0.0f, -1.0f});
                const vec3 right = glm::normalize(rotation * vec3{1.0f, 0.0f, 0.0f});
                vec3 forward_xz = forward;
                forward_xz.y = 0.0f;
                if (glm::dot(forward_xz, forward_xz) < 1e-10f)
                    forward_xz = vec3{0.0f, 0.0f, -1.0f};
                else
                    forward_xz = glm::normalize(forward_xz);
                const vec3 right_xz = glm::normalize(glm::cross(forward_xz, k_world_up));
                const float pan = k_pan_units_per_sec * static_cast<float>(delta_sec);
                vec3 pivot_delta{0.0f};
                if (key_down(input.current.keys, GLFW_KEY_UP)) pivot_delta += forward_xz * pan;
                if (key_down(input.current.keys, GLFW_KEY_DOWN)) pivot_delta -= forward_xz * pan;
                if (key_down(input.current.keys, GLFW_KEY_LEFT)) pivot_delta -= right_xz * pan;
                if (key_down(input.current.keys, GLFW_KEY_RIGHT)) pivot_delta += right_xz * pan;
                if (key_down(input.current.keys, GLFW_KEY_E)) pivot_delta += k_world_up * pan;
                if (key_down(input.current.keys, GLFW_KEY_Q)) pivot_delta -= k_world_up * pan;
                orbit->pivot += pivot_delta;
            }

            orbit->distance = std::clamp(orbit->distance, k_distance_min, k_distance_max);
            auto node = with<scene::Node>::modify(context, self);
            apply_pose(*node, *orbit);
        }

    } // namespace

    auto CameraOrbit::Actions::create(Writing context, scene::Camera::Id anchor, Pos pivot, float distance) -> Id {
        distance = std::clamp(distance, k_distance_min, k_distance_max);
        const auto& node = with<scene::Node>::get(context, anchor);
        HPB hpb = node.pose.hpb();
        hpb.z = 0.0f;
        const vec3 to_camera = node.pose.position - pivot;
        if (glm::dot(to_camera, to_camera) > 1e-8f) {
            distance = std::clamp(glm::length(to_camera), k_distance_min, k_distance_max);
            const vec3 forward = glm::normalize(-to_camera);
            const float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
            const float heading = std::atan2(forward.x, -forward.z);
            hpb = HPB{glm::degrees(heading), glm::degrees(pitch), 0.0f};
        }

        CameraOrbit::Quantum quantum{.pivot = pivot, .hpb = hpb, .distance = distance};
        with<CameraOrbit>::extend(context, anchor, quantum);
        auto writable = with<scene::Node>::modify(context, anchor);
        apply_pose(*writable, quantum);
        return anchor;
    }

    struct CameraOrbit::Internals : CameraOrbit::DefaultInternals {
        static void update(Reacting context) {
            for (const auto& change : context.changes<system::Clock>().updated()) {
                const int64 dt_us = change.now.absolute - change.old.absolute;
                if (dt_us <= 0)
                    continue;
                const seconds delta_sec = static_cast<seconds>(dt_us) / 1'000'000.0;
                for (const auto entry : context.proposal.aspect<system::Window>().items()) {
                    for (const auto [id, _] : context.proposal.aspect<CameraOrbit>().items()) {
                        drive(context, id, entry.id, delta_sec);
                    }
                }
            }
        }
    };

    auto CameraOrbit::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<CameraOrbit, system::Clock>(&CameraOrbit::Internals::update),
        };
    }

}
