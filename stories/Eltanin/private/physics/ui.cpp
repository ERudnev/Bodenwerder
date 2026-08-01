#include "physics/ui.h"

#include <eltanin/resources/geometry.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <base/logging.h>

#include <cmath>

#include <imgui.h>

namespace eltanin::phys {

    namespace {

        // Diamond half-extent as fraction of horizontal half-FOV → constant screen fraction (FOV-aware).
        constexpr float k_particle_view_fraction = 0.004375f;

        auto first_root(Reading context) -> base::maybe<rmmr::scene::Root::Id> {
            for (const auto [id, _] : context->aspect<rmmr::scene::Root>().items()) {
                return id;
            }
            return {};
        }

        auto locator_from(const rmmr::Pose& pose) -> rmmr::Locator {
            return rmmr::Locator{.pos = pose.position, .euler = pose.hpb()};
        }

        void destroy_actor(Writing context, rmmr::scene::actor::Simple::Id actor) {
            for (const auto [root, group] : context->aspect<rmmr::scene::Node_group>().items()) {
                if (group.contains(actor)) {
                    with<rmmr::scene::Node_group>::deleteElement(context, root, actor);
                    return;
                }
            }
            if (with<rmmr::scene::Node>::exists(context, actor)) {
                with<rmmr::scene::Node>::remove(context, actor);
            }
        }

        auto particle_angular_scale(Reading context, const vec3& world_pos) -> float {
            const auto& world = with<World>::get_global(context);
            if (not world.camera) {
                return 0.15f;
            }
            const auto camera = *world.camera;
            const auto& cam = with<rmmr::scene::Camera>::get(context, camera);
            const auto& cam_node = with<rmmr::scene::Node>::get(context, camera);
            const float distance = std::max(glm::length(world_pos - cam_node.pose.position), cam.z_near);
            return distance * std::tan(cam.fov_x * 0.5f * k_particle_view_fraction);
        }

    } // namespace

    void Ui::onDrawEnabled(Writing context) {
        base::message("eltanin::phys::Ui: draw shapes enabled");
        state.actors.clear();
        state.particles.clear();
        bodies.clear();
        particleIds.clear();

        const auto root = first_root(context);
        if (not root) {
            base::message("eltanin::phys::Ui: no scene Root; skip shape actors");
            return;
        }

        const auto device = with<World>::get_global(context).window;
        if (shapeMaterial) {
            for (const auto [atomic_id, atomic] : context->aspect<Atomic>().items()) {
                const auto& shape = with<resource::atomic::Asset>::get(context, atomic.shape);
                if (device) {
                    with<::eltanin::resources::AtomicVisualizer>::materialize(context, shape.visualizer, *device);
                }
                const auto actor = with<rmmr::scene::Interface>::createSimpleActor(context, *root, locator_from(atomic.restored), item<rmmr::scene::actor::Simple>{
                    .geometry = shape.visualizer,
                    .material = *shapeMaterial,
                    .albedo = rmmr::RGB{1.0f, 1.0f, 1.0f},
                });
                state.actors.push_back(actor);
                bodies.push_back(atomic_id);
            }
        } else {
            base::message("eltanin::phys::Ui: shapeMaterial unset; skip Atomic actors");
        }

        if (particleGeometry and particleMaterial) {
            for (const auto [particle_id, particle] : context->aspect<Particle>().items()) {
                const float scale = particle_angular_scale(context, particle.current);
                const auto actor = with<rmmr::scene::Interface>::createSimpleActor(context, *root, rmmr::Locator{.pos = particle.current, .euler = HPB{0.0f, 0.0f, 0.0f}}, item<rmmr::scene::actor::Simple>{
                    .geometry = *particleGeometry,
                    .material = *particleMaterial,
                    .albedo = rmmr::RGB{1.0f, 1.0f, 1.0f},
                    .scale = vec3{scale, scale, scale},
                });
                state.particles.push_back(actor);
                particleIds.push_back(particle_id);
            }
        } else {
            base::message("eltanin::phys::Ui: particleGeometry/Material unset; skip Particle actors");
        }
    }

    void Ui::onDrawDisabled(Writing context) {
        base::message("eltanin::phys::Ui: draw shapes disabled");
        for (const auto actor : state.actors) {
            destroy_actor(context, actor);
        }
        for (const auto actor : state.particles) {
            destroy_actor(context, actor);
        }
        state.actors.clear();
        state.particles.clear();
        bodies.clear();
        particleIds.clear();
    }

    void Ui::onDraw(Writing context) {
        std::size_t slot = 0;
        while (slot < state.actors.size()) {
            const auto actor = state.actors[slot];
            const auto body = bodies[slot];
            if (not with<Atomic>::exists(context, body) or not with<rmmr::scene::actor::Simple>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Simple>::exists(context, actor)) {
                    destroy_actor(context, actor);
                }
                state.actors.erase(state.actors.begin() + static_cast<std::ptrdiff_t>(slot));
                bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            with<rmmr::scene::Node>::modify(context, actor)->pose = with<Atomic>::get(context, body).restored;
            ++slot;
        }

        slot = 0;
        while (slot < state.particles.size()) {
            const auto actor = state.particles[slot];
            const auto particle_id = particleIds[slot];
            if (not with<Particle>::exists(context, particle_id) or not with<rmmr::scene::actor::Simple>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Simple>::exists(context, actor)) {
                    destroy_actor(context, actor);
                }
                state.particles.erase(state.particles.begin() + static_cast<std::ptrdiff_t>(slot));
                particleIds.erase(particleIds.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            const auto& particle = with<Particle>::get(context, particle_id);
            auto node = with<rmmr::scene::Node>::modify(context, actor);
            node->pose.position = particle.current;
            node->pose.rotation = quat{1.0f, 0.0f, 0.0f, 0.0f};
            const float scale = particle_angular_scale(context, particle.current);
            with<rmmr::scene::actor::Simple>::modify(context, actor)->scale = vec3{scale, scale, scale};
            ++slot;
        }
    }

    void Ui::draw(Writing context, bool& open, System& system) {
        if (open) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(
                ImVec2{viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, viewport->WorkPos.y + 48.0f},
                ImGuiCond_FirstUseEver,
                ImVec2{1.0f, 0.0f});
            ImGui::SetNextWindowSize(ImVec2{420.0f, 0.0f}, ImGuiCond_FirstUseEver);

            if (ImGui::Begin("Physics", &open)) {
                ImGui::TextUnformatted("Time scale");
                constexpr float k_scales[] = {
                    1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f,
                    1.0f,
                    2.0f, 4.0f, 8.0f, 16.0f,
                };
                constexpr const char* k_labels[] = {
                    "1/16", "1/8", "1/4", "1/2",
                    "1",
                    "x2", "x4", "x8", "x16",
                };
                int selected = 4;
                for (int i = 0; i < 9; ++i) {
                    if (system.state.time_scale == k_scales[i]) {
                        selected = i;
                    }
                }
                for (int i = 0; i < 9; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::RadioButton(k_labels[i], selected == i)) {
                        system.state.time_scale = k_scales[i];
                    }
                }
            }
            ImGui::End();
        }

        if (open and not prevOpen) {
            onDrawEnabled(context);
        } else if (not open and prevOpen) {
            onDrawDisabled(context);
        }
        prevOpen = open;

        if (open) {
            onDraw(context);
        }
    }

}
