#include "physics/ui.h"
#include "mech/semantics/together.include.h"

#include <eltanin/world.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <base/logging.h>

#include <imgui.h>

#include <utility>

namespace eltanin::phys {

    namespace {

        constexpr float particleWorldScale = 0.1f; // fixed debug diamond size (meters)

        auto first_root(Reading context) -> base::maybe<rmmr::scene::Root::Id> {
            for (const auto [id, _] : context->aspect<rmmr::scene::Root>().items()) {
                return id;
            }
            return {};
        }

        void destroy_actor(Writing context, rmmr::scene::actor::Mesh::Id actor) {
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

    } // namespace

    void Ui::enableColliders(Writing context) {
        disableColliders(context);
        const auto root = first_root(context);
        if (not root) {
            base::message("eltanin::phys::Ui: no scene Root; skip collider actors");
            return;
        }
        if (not shapeMaterial or not shapeTexpack or not shapeAlbedoLayer or not shapeGeometry) {
            base::message("eltanin::phys::Ui: shapeMaterial/Texpack/Layer/Geometry unset; skip collider actors");
            return;
        }
        const auto edge = mech::physical::edgeMeters;
        const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
            .geometry = *shapeGeometry,
            .entry = rmmr::resource::geometry::EntryId{0},
            .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = *shapeMaterial, .textures = {{"albedoMap", *shapeAlbedoLayer}}}}},
            .texpack = *shapeTexpack,
        };
        const auto appearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{edge, edge, edge});
        for (const auto [atomic_id, atomic] : context->aspect<Atomic>().items()) {
            state.actors.push_back(with<rmmr::scene::Interface>::createMeshActor(context, *root, atomic.restored, resolved, appearance));
            bodies.push_back(atomic_id);
        }
    }

    void Ui::disableColliders(Writing context) {
        for (const auto actor : state.actors) {
            destroy_actor(context, actor);
        }
        state.actors.clear();
        bodies.clear();
    }

    void Ui::enableParticles(Writing context) {
        disableParticles(context);
        const auto root = first_root(context);
        if (not root) {
            base::message("eltanin::phys::Ui: no scene Root; skip particle actors");
            return;
        }
        if (not particleGeometry or not particleMaterial) {
            base::message("eltanin::phys::Ui: particleGeometry/Material unset; skip particle actors");
            return;
        }
        const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
            .geometry = *particleGeometry,
            .entry = rmmr::resource::geometry::EntryId{0},
            .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = *particleMaterial, .textures = {}}}},
            .texpack = {},
        };
        const auto appearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{particleWorldScale, particleWorldScale, particleWorldScale});
        for (const auto [particle_id, particle] : context->aspect<Particle>().items()) {
            state.particles.push_back(with<rmmr::scene::Interface>::createMeshActor(context, *root, rmmr::Pose::from(particle.current, HPB{0.0f, 0.0f, 0.0f}), resolved, appearance));
            particleIds.push_back(particle_id);
        }
    }

    void Ui::disableParticles(Writing context) {
        for (const auto actor : state.particles) {
            destroy_actor(context, actor);
        }
        state.particles.clear();
        particleIds.clear();
    }

    void Ui::syncColliders(Writing context) {
        std::size_t slot = 0;
        while (slot < state.actors.size()) {
            const auto actor = state.actors[slot];
            const auto body = bodies[slot];
            if (not with<Atomic>::exists(context, body) or not with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                    destroy_actor(context, actor);
                }
                state.actors.erase(state.actors.begin() + static_cast<std::ptrdiff_t>(slot));
                bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            with<rmmr::scene::Node>::modify(context, actor)->pose = with<Atomic>::get(context, body).restored;
            ++slot;
        }
    }

    void Ui::syncParticles(Writing context) {
        std::size_t slot = 0;
        while (slot < state.particles.size()) {
            const auto actor = state.particles[slot];
            const auto particle_id = particleIds[slot];
            if (not with<Particle>::exists(context, particle_id) or not with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
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
                constexpr float scales[] = {
                    1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f,
                    1.0f,
                    2.0f, 4.0f, 8.0f, 16.0f,
                };
                constexpr const char* labels[] = {
                    "1/16", "1/8", "1/4", "1/2",
                    "1",
                    "x2", "x4", "x8", "x16",
                };
                int selected = 4;
                for (int i = 0; i < 9; ++i) {
                    if (system.state.time_scale == scales[i]) {
                        selected = i;
                    }
                }
                for (int i = 0; i < 9; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::RadioButton(labels[i], selected == i)) {
                        system.state.time_scale = scales[i];
                    }
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Debug draw");
                ImGui::Checkbox("Colliders", &showColliders);
                ImGui::SameLine();
                ImGui::Checkbox("Particles", &showParticles);
            }
            ImGui::End();
        }

        if (not open) {
            showColliders = false;
            showParticles = false;
        }

        if (showColliders and not prevShowColliders) {
            enableColliders(context);
        } else if (not showColliders and prevShowColliders) {
            disableColliders(context);
        }
        if (showParticles and not prevShowParticles) {
            enableParticles(context);
        } else if (not showParticles and prevShowParticles) {
            disableParticles(context);
        }
        prevShowColliders = showColliders;
        prevShowParticles = showParticles;

        if (showColliders) {
            syncColliders(context);
        }
        if (showParticles) {
            syncParticles(context);
        }
    }

}
