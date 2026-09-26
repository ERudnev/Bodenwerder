#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/viewInput.q1.h>

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
        constexpr float k_heading_scale_x = 1.0f;
        constexpr float k_pitch_min_deg = -89.0f;
        constexpr float k_pitch_max_deg = 89.0f;
        constexpr float k_distance_min = 0.5f;
        constexpr float k_distance_max = 500.0f;
        constexpr float k_zoom_wheel_base = 0.85f; // distance *= base^wheel (scroll up → closer)
        constexpr float panViewPerSec = 0.1f; // world/sec = this × orbit distance, so arrows track zoom

        const glm::vec3 k_world_up{0.0f, 1.0f, 0.0f};

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        auto button_down(const vector<bool>& buttons, int button) -> bool {
            return static_cast<std::size_t>(button) < buttons.size() && buttons[static_cast<std::size_t>(button)];
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

        void drive(Writing context, CameraOrbit::Id self, seconds delta_sec) {
            const auto& mail = with<system::ViewInput>::get(context, with<CameraOrbit>::get(context, self).input);
            if (not mail.engaged)
                return;
            auto orbit = with<CameraOrbit>::modify(context, self);

            if (button_down(mail.buttons, GLFW_MOUSE_BUTTON_RIGHT)) {
                orbit->hpb.x += k_heading_scale_x * static_cast<float>(mail.mouseShift.x) * k_mouse_sens_deg_per_pixel;
                orbit->hpb.y += -static_cast<float>(mail.mouseShift.y) * k_mouse_sens_deg_per_pixel;
                orbit->hpb.y = std::clamp(orbit->hpb.y, k_pitch_min_deg, k_pitch_max_deg);
                orbit->hpb.z = 0.0f;
            }

            if (std::abs(mail.wheel) > 1e-6f)
                orbit->distance *= std::pow(k_zoom_wheel_base, mail.wheel);

            if (delta_sec > 0.0) {
                const quat rotation = rotation_from_orbit_hpb(orbit->hpb);
                const vec3 forward = glm::normalize(rotation * vec3{0.0f, 0.0f, -1.0f});
                vec3 forward_xz = forward;
                forward_xz.y = 0.0f;
                if (glm::dot(forward_xz, forward_xz) < 1e-10f)
                    forward_xz = vec3{0.0f, 0.0f, -1.0f};
                else
                    forward_xz = glm::normalize(forward_xz);
                const vec3 right_xz = glm::normalize(glm::cross(forward_xz, k_world_up));
                const float pan = panViewPerSec * orbit->distance * static_cast<float>(delta_sec);
                vec3 pivot_delta{0.0f};
                if (key_down(mail.keys, GLFW_KEY_UP)) pivot_delta += forward_xz * pan;
                if (key_down(mail.keys, GLFW_KEY_DOWN)) pivot_delta -= forward_xz * pan;
                if (key_down(mail.keys, GLFW_KEY_LEFT)) pivot_delta -= right_xz * pan;
                if (key_down(mail.keys, GLFW_KEY_RIGHT)) pivot_delta += right_xz * pan;
                if (key_down(mail.keys, GLFW_KEY_PAGE_UP)) pivot_delta.y += pan;
                if (key_down(mail.keys, GLFW_KEY_PAGE_DOWN)) pivot_delta.y -= pan;
                orbit->pivot += pivot_delta;
            }

            orbit->distance = std::clamp(orbit->distance, k_distance_min, k_distance_max);
            auto node = with<scene::Node>::modify(context, self);
            apply_pose(*node, *orbit);
        }

    } // namespace

    auto CameraOrbit::Actions::create(Writing context, scene::Camera::Id anchor, system::ViewInput::Id input, Pos pivot, float distance) -> Id {
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

        CameraOrbit::Quantum quantum{.pivot = pivot, .hpb = hpb, .distance = distance, .input = input};
        with<CameraOrbit>::extend(context, anchor, quantum);
        auto writable = with<scene::Node>::modify(context, anchor);
        apply_pose(*writable, quantum);
        return anchor;
    }

    void CameraOrbit::Actions::tick(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        // ids first: drive writes into the same session
        std::vector<Id> cameras;
        for (const auto entry : context->aspect<CameraOrbit>().items())
            cameras.push_back(entry.id);
        for (const auto camera : cameras)
            drive(context, camera, dt);
    }

    auto doctrine::cameraOrbit() -> Schema {
        return ask::schema::aspect<CameraOrbit>();
    }

}
