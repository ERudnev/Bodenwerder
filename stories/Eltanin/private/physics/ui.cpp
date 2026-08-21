#include "physics/ui.h"
#include "physics/compound.h"

#include <eltanin/world.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/window.q1.h>

#include <base/logging.h>

#include <imgui.h>

#include <utility>

namespace eltanin::phys {

    Ui::Ui(rmmr::resource::material::Asset::Id shapeMaterial,
           rmmr::resource::texpack::Pack::Id shapeTexpack,
           std::string shapeAlbedoLayer,
           rmmr::resource::geometry::Asset::Id shapeGeometry,
           rmmr::resource::geometry::Asset::Id particleGeometry,
           rmmr::resource::material::Asset::Id particleMaterial)
        : shapeMaterial(shapeMaterial)
        , shapeTexpack(shapeTexpack)
        , shapeAlbedoLayer(std::move(shapeAlbedoLayer))
        , shapeGeometry(shapeGeometry)
        , particleGeometry(particleGeometry)
        , particleMaterial(particleMaterial)
        , showColliders(false)
        , showParticles(false)
        , prevShowColliders(false)
        , prevShowParticles(false) {
    }

    namespace {

        constexpr float particleWorldScale = 0.1f; // fixed debug diamond size (meters)

        auto first_root(Reading context) -> base::maybe<rmmr::scene::Root::Id> {
            for (const auto [id, _] : context->aspect<rmmr::scene::Root>().items()) {
                return id;
            }
            return {};
        }

        auto first_device(Reading context) -> base::maybe<rmmr::system::Device::Id> {
            for (const auto [id, _] : context->aspect<rmmr::system::Window>().items()) {
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
        const auto device = first_device(context);
        if (not root or not device) {
            base::message("eltanin::phys::Ui: no scene Root/Device; skip collision actors");
            return;
        }
        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        if (not manager) {
            base::message("eltanin::phys::Ui: no resource Manager; skip collision actors");
            return;
        }
        if (not with<rmmr::resource::Unit_group>::exists(context, *manager))
            with<rmmr::resource::Unit_group>::extend(context, *manager);
        auto appearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        for (const auto [crystalId, crystal] : context->aspect<rigid::Crystal>().items()) {
            if (not with<rigid::Collision>::exists(context, crystalId))
                continue;
            const auto cpu = rigid::debugMeshFromCompound(with<rigid::Collision>::get(context, crystalId).compound, crystal.shape);
            if (cpu.positions.empty() or cpu.indices.empty())
                continue;
            const auto geometryId = with<rmmr::resource::Unit_group>::addElement(context, *manager, rmmr::resource::Unit::Quantum{.name = rmmr::resource::Unit::Name::from("Eltanin", "collision")});
            with<rmmr::resource::geometry::Asset>::extend(context, geometryId, rmmr::resource::geometry::Asset::Quantum{});
            if (not with<rmmr::resource::geometry::Asset>::install(context, geometryId, *device, cpu))
                continue;
            const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
                .geometry = geometryId,
                .entry = rmmr::resource::geometry::EntryId{0},
                .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = shapeMaterial, .textures = {{"albedoMap", shapeAlbedoLayer}}}}},
                .texpack = shapeTexpack,
            };
            state.actors.push_back(with<rmmr::scene::Interface>::createMeshActor(context, *root, crystal.restored.pose(), resolved, appearance));
            bodies.push_back(crystalId);
            colliderGeometries.push_back(geometryId);
        }
    }

    void Ui::disableColliders(Writing context) {
        for (const auto actor : state.actors) {
            destroy_actor(context, actor);
        }
        state.actors.clear();
        bodies.clear();
        colliderGeometries.clear();
    }

    void Ui::enableParticles(Writing context) {
        disableParticles(context);
        const auto root = first_root(context);
        if (not root) {
            base::message("eltanin::phys::Ui: no scene Root; skip particle actors");
            return;
        }
        const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
            .geometry = particleGeometry,
            .entry = rmmr::resource::geometry::EntryId{0},
            .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = particleMaterial, .textures = {}}}},
            .texpack = {},
        };
        const auto appearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{particleWorldScale, particleWorldScale, particleWorldScale});
        for (const auto [crystalId, crystal] : context->aspect<rigid::Crystal>().items()) {
            for (std::size_t index = 0; index < crystal.particles.size(); ++index) {
                state.particles.push_back(with<rmmr::scene::Interface>::createMeshActor(context, *root, rmmr::Pose::from(crystal.particles[index].position, HPB{0.0f, 0.0f, 0.0f}), resolved, appearance));
                particleRefs.push_back(ParticleRef{.crystal = crystalId, .index = index});
            }
        }
    }

    void Ui::disableParticles(Writing context) {
        for (const auto actor : state.particles) {
            destroy_actor(context, actor);
        }
        state.particles.clear();
        particleRefs.clear();
    }

    void Ui::syncColliders(Writing context) {
        std::size_t slot = 0;
        while (slot < state.actors.size()) {
            const auto actor = state.actors[slot];
            const auto body = bodies[slot];
            if (not with<rigid::Crystal>::exists(context, body) or not with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                    destroy_actor(context, actor);
                }
                state.actors.erase(state.actors.begin() + static_cast<std::ptrdiff_t>(slot));
                bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            with<rmmr::scene::Node>::modify(context, actor)->pose = with<rigid::Crystal>::get(context, body).restored.pose();
            ++slot;
        }
    }

    void Ui::syncParticles(Writing context) {
        std::size_t slot = 0;
        while (slot < state.particles.size()) {
            const auto actor = state.particles[slot];
            const ParticleRef particleRef = particleRefs[slot];
            const bool missingCrystal = not with<rigid::Crystal>::exists(context, particleRef.crystal);
            const bool missingParticle = not missingCrystal and particleRef.index >= with<rigid::Crystal>::get(context, particleRef.crystal).particles.size();
            if (missingCrystal or missingParticle or not with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Mesh>::exists(context, actor)) {
                    destroy_actor(context, actor);
                }
                state.particles.erase(state.particles.begin() + static_cast<std::ptrdiff_t>(slot));
                particleRefs.erase(particleRefs.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            const Particle& particle = with<rigid::Crystal>::get(context, particleRef.crystal).particles[particleRef.index];
            auto node = with<rmmr::scene::Node>::modify(context, actor);
            node->pose.position = particle.position;
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
                    if (system.state.timeScale == scales[i]) {
                        selected = i;
                    }
                }
                for (int i = 0; i < 9; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::RadioButton(labels[i], selected == i)) {
                        system.state.timeScale = scales[i];
                    }
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Debug draw");
                ImGui::Checkbox("Collisions", &showColliders);
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
