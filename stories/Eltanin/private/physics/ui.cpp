#include "physics/ui.h"

#include <eltanin/entities/block.q1.h>
#include <eltanin/geo/boulder.q1.h>
#include <eltanin/geo/rock.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/geometry.h>
#include <rmmr/system/window.q1.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <imgui.h>
#include <string>
#include <utility>

namespace eltanin::phys {

    Ui::Ui(rmmr::resource::geometry::Asset::Id particleGeometry,
           rmmr::resource::geometry::Asset::Id sphereGeometry,
           rmmr::resource::material::Asset::Id particleMaterial,
           rmmr::resource::material::Asset::Id hullMaterial,
           rmmr::resource::texpack::Pack::Id hullTexpack)
        : particleGeometry(particleGeometry)
        , sphereGeometry(sphereGeometry)
        , particleMaterial(particleMaterial)
        , hullMaterial(hullMaterial)
        , hullTexpack(hullTexpack)
        , showParticles(false)
        , prevShowParticles(false)
        , showHulls(false)
        , prevShowHulls(false) {
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

        auto combatActorOf(Reading context, rigid::Crystal::Id crystal) -> base::maybe<rmmr::scene::actor::Mesh::Id> {
            for (const auto [_, rock] : context->aspect<geo::Rock>().items()) {
                if (rock.body == crystal)
                    return rock.actor;
            }
            for (const auto [_, block] : context->aspect<Block>().items()) {
                if (block.body == crystal)
                    return block.actor;
            }
            return {};
        }

        auto combatActorOf(Reading context, rigid::Ball::Id ball) -> base::maybe<rmmr::scene::actor::Mesh::Id> {
            for (const auto [_, boulder] : context->aspect<geo::Boulder>().items()) {
                if (boulder.ball == ball)
                    return boulder.actor;
            }
            return {};
        }

        auto planarAxes(vec3 normal, vec3 along) -> std::pair<vec3, vec3> {
            vec3 u = along - normal * glm::dot(along, normal);
            if (glm::dot(u, u) < 1.0e-10f) {
                const vec3 fallback = glm::abs(normal.y) < 0.9f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f};
                u = fallback - normal * glm::dot(fallback, normal);
            }
            u = glm::normalize(u);
            return {u, glm::normalize(glm::cross(normal, u))};
        }

        auto debugMeshFromHull(const rigid::Compound::Hull& hull, const vector<vec3>& shape) -> rmmr::resource::builders::geometry::CpuPresentation {
            using rmmr::resource::builders::geometry::CpuPresentation;
            CpuPresentation cpu{
                .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"}),
                .positions = {},
                .normals = {},
                .uv0 = {},
                .color0 = {},
                .indices = {},
                .mix0 = {},
            };
            for (const auto& face : hull.faces) {
                if (face.points.size() != 3)
                    continue;
                const integer ia = face.points[0];
                const integer ib = face.points[1];
                const integer ic = face.points[2];
                if (ia < 0 or ib < 0 or ic < 0)
                    continue;
                if (static_cast<std::size_t>(ia) >= shape.size() or static_cast<std::size_t>(ib) >= shape.size() or static_cast<std::size_t>(ic) >= shape.size())
                    continue;
                const vec3 a = shape[static_cast<std::size_t>(ia)];
                const vec3 b = shape[static_cast<std::size_t>(ib)];
                const vec3 c = shape[static_cast<std::size_t>(ic)];
                vec3 normal = face.normal;
                if (glm::dot(normal, normal) < 1.0e-12f)
                    normal = glm::cross(b - a, c - a);
                const float mag = glm::length(normal);
                if (mag <= 1.0e-12f)
                    continue;
                normal /= mag;
                const auto [uAxis, vAxis] = planarAxes(normal, b - a);
                cpu.positions.push_back(a);
                cpu.positions.push_back(b);
                cpu.positions.push_back(c);
                cpu.normals.push_back(normal);
                cpu.normals.push_back(normal);
                cpu.normals.push_back(normal);
                cpu.uv0.push_back(rmmr::UV{glm::dot(a, uAxis), glm::dot(a, vAxis)});
                cpu.uv0.push_back(rmmr::UV{glm::dot(b, uAxis), glm::dot(b, vAxis)});
                cpu.uv0.push_back(rmmr::UV{glm::dot(c, uAxis), glm::dot(c, vAxis)});
            }
            return cpu;
        }

    } // namespace

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

    void Ui::hideCombat(Writing context) {
        restoreCombat(context);
        for (const auto& hullRef : hullRefs) {
            base::maybe<rmmr::scene::actor::Mesh::Id> actor;
            if (hullRef.crystal)
                actor = combatActorOf(context, *hullRef.crystal);
            else if (hullRef.ball)
                actor = combatActorOf(context, *hullRef.ball);
            if (not actor or not with<rmmr::scene::actor::MeshState>::exists(context, *actor))
                continue;
            rmmr::scene::actor::MeshState::Actions::setVisible(context, *actor, false);
            hiddenCombat.push_back(*actor);
        }
    }

    void Ui::restoreCombat(Writing context) {
        for (const auto actor : hiddenCombat) {
            if (with<rmmr::scene::actor::MeshState>::exists(context, actor))
                rmmr::scene::actor::MeshState::Actions::setVisible(context, actor, true);
        }
        hiddenCombat.clear();
    }

    void Ui::enableHulls(Writing context) {
        disableHulls(context);
        const auto root = first_root(context);
        const auto device = first_device(context);
        if (not root or not device) {
            base::message("eltanin::phys::Ui: no Root/Device; skip hull actors");
            return;
        }
        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        const auto appearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        for (const auto [crystalId, crystal] : context->aspect<rigid::Crystal>().items()) {
            auto cpu = debugMeshFromHull(crystal.hull, crystal.shape);
            if (cpu.positions.empty())
                continue;
            const auto geometryId = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = rmmr::resource::Unit::Name::from("Eltanin", "hullDebug")});
            with<rmmr::resource::geometry::Asset>::extend(context, geometryId, rmmr::resource::geometry::Asset::Quantum{});
            if (not with<rmmr::resource::geometry::Asset>::install(context, geometryId, *device, cpu)) {
                base::message("eltanin::phys::Ui: hull debug geometry install failed");
                continue;
            }
            const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
                .geometry = geometryId,
                .entry = rmmr::resource::geometry::EntryId{0},
                .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = hullMaterial, .textures = {{"albedoMap", "debug02.jpg"}}}}},
                .texpack = hullTexpack,
            };
            state.hulls.push_back(with<rmmr::scene::Interface>::createMeshActor(context, *root, crystal.restored.pose(), resolved, appearance));
            hullRefs.push_back(HullRef{.crystal = crystalId, .ball = {}});
        }
        for (const auto [ballId, ball] : context->aspect<rigid::Ball>().items()) {
            const float radius = ball.body.radius;
            if (radius <= 0.0f)
                continue;
            const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
                .geometry = sphereGeometry,
                .entry = rmmr::resource::geometry::EntryId{0},
                .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = hullMaterial, .textures = {{"albedoMap", "debug02.jpg"}}}}},
                .texpack = hullTexpack,
            };
            const auto ballAppearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{radius, radius, radius});
            state.hulls.push_back(with<rmmr::scene::Interface>::createMeshActor(context, *root, ball.body.pose(), resolved, ballAppearance));
            hullRefs.push_back(HullRef{.crystal = {}, .ball = ballId});
        }
        hideCombat(context);
    }

    void Ui::disableHulls(Writing context) {
        restoreCombat(context);
        for (const auto actor : state.hulls) {
            destroy_actor(context, actor);
        }
        state.hulls.clear();
        hullRefs.clear();
    }

    void Ui::syncHulls(Writing context) {
        std::size_t slot = 0;
        while (slot < state.hulls.size()) {
            const auto actor = state.hulls[slot];
            const HullRef hullRef = hullRefs[slot];
            const bool missingActor = not with<rmmr::scene::actor::Mesh>::exists(context, actor);
            if (hullRef.crystal) {
                const bool missingCrystal = not with<rigid::Crystal>::exists(context, *hullRef.crystal);
                if (missingCrystal or missingActor) {
                    if (with<rmmr::scene::actor::Mesh>::exists(context, actor))
                        destroy_actor(context, actor);
                    state.hulls.erase(state.hulls.begin() + static_cast<std::ptrdiff_t>(slot));
                    hullRefs.erase(hullRefs.begin() + static_cast<std::ptrdiff_t>(slot));
                    continue;
                }
                with<rmmr::scene::Node>::modify(context, actor)->pose = with<rigid::Crystal>::get(context, *hullRef.crystal).restored.pose();
            } else if (hullRef.ball) {
                const bool missingBall = not with<rigid::Ball>::exists(context, *hullRef.ball);
                if (missingBall or missingActor) {
                    if (with<rmmr::scene::actor::Mesh>::exists(context, actor))
                        destroy_actor(context, actor);
                    state.hulls.erase(state.hulls.begin() + static_cast<std::ptrdiff_t>(slot));
                    hullRefs.erase(hullRefs.begin() + static_cast<std::ptrdiff_t>(slot));
                    continue;
                }
                with<rmmr::scene::Node>::modify(context, actor)->pose = with<rigid::Ball>::get(context, *hullRef.ball).body.pose();
            } else {
                if (with<rmmr::scene::actor::Mesh>::exists(context, actor))
                    destroy_actor(context, actor);
                state.hulls.erase(state.hulls.begin() + static_cast<std::ptrdiff_t>(slot));
                hullRefs.erase(hullRefs.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
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
                ImGui::Checkbox("Particles", &showParticles);
                ImGui::Checkbox("Collisions", &showHulls);
            }
            ImGui::End();
        }

        if (not open) {
            showParticles = false;
            showHulls = false;
        }

        if (showParticles and not prevShowParticles) {
            enableParticles(context);
        } else if (not showParticles and prevShowParticles) {
            disableParticles(context);
        }
        prevShowParticles = showParticles;

        if (showHulls and not prevShowHulls) {
            enableHulls(context);
        } else if (not showHulls and prevShowHulls) {
            disableHulls(context);
        }
        prevShowHulls = showHulls;

        if (showParticles)
            syncParticles(context);
        if (showHulls)
            syncHulls(context);
    }

}
