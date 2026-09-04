#include "physics/ui.h"

#include <eltanin/locality/thing.q1.h>
#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
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
#include <unordered_set>
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

        auto bodyLinearSpeed(const rigid::Crystal::Quantum& crystal) -> float {
            dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (const Particle& particle : crystal.particles) {
                moment += (particle.position - particle.prev) / Settings::fixedStep * double(particle.mass);
                mass += double(particle.mass);
            }
            if (mass <= 0.0)
                return 0.0f;
            return static_cast<float>(glm::length(moment / mass));
        }

        auto productionActorOf(Reading context, Body::Id body) -> base::maybe<rmmr::scene::actor::Mesh::Id> {
            for (const auto [_, rock] : context->aspect<locality::geo::Rock>().items()) {
                if (rock.body == body)
                    return rock.actor;
            }
            for (const auto [_, construct] : context->aspect<locality::Construct>().items()) {
                if (construct.body == body)
                    return construct.actor;
            }
            for (const auto [_, boulder] : context->aspect<locality::geo::Boulder>().items()) {
                if (boulder.body == body)
                    return boulder.actor;
            }
            for (const auto [_, scrap] : context->aspect<locality::Scrap>().items()) {
                if (scrap.body == body)
                    return scrap.actor;
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

        auto debugMeshFromHull(const rigid::Hull& hull, const vector<vec3>& shape) -> rmmr::resource::builders::geometry::CpuPresentation {
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
                if (face.points.size() < 3)
                    continue;
                bool valid = true;
                for (const integer id : face.points) {
                    if (id < 0 or static_cast<std::size_t>(id) >= shape.size()) {
                        valid = false;
                        break;
                    }
                }
                if (not valid)
                    continue;
                const vec3 a = shape[static_cast<std::size_t>(face.points[0])];
                const vec3 b = shape[static_cast<std::size_t>(face.points[1])];
                const vec3 c = shape[static_cast<std::size_t>(face.points[2])];
                vec3 normal = face.normal;
                if (glm::dot(normal, normal) < 1.0e-12f)
                    normal = glm::cross(b - a, c - a);
                const float mag = glm::length(normal);
                if (mag <= 1.0e-12f)
                    continue;
                normal /= mag;
                const auto [uAxis, vAxis] = planarAxes(normal, b - a);
                for (std::size_t i = 1; i + 1 < face.points.size(); ++i) {
                    const vec3 p1 = shape[static_cast<std::size_t>(face.points[i])];
                    const vec3 p2 = shape[static_cast<std::size_t>(face.points[i + 1])];
                    cpu.positions.push_back(a);
                    cpu.positions.push_back(p1);
                    cpu.positions.push_back(p2);
                    cpu.normals.push_back(normal);
                    cpu.normals.push_back(normal);
                    cpu.normals.push_back(normal);
                    cpu.uv0.push_back(rmmr::UV{glm::dot(a, uAxis), glm::dot(a, vAxis)});
                    cpu.uv0.push_back(rmmr::UV{glm::dot(p1, uAxis), glm::dot(p1, vAxis)});
                    cpu.uv0.push_back(rmmr::UV{glm::dot(p2, uAxis), glm::dot(p2, vAxis)});
                }
            }
            return cpu;
        }

        void censusRow(const char* name, std::size_t n) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name);
            ImGui::TableNextColumn();
            ImGui::Text("%d", static_cast<int>(n));
        }

    } // namespace

    void Ui::enableParticles(Writing context, rmmr::scene::Root::Id root) {
        disableParticles(context);
        if (not with<rmmr::scene::Root>::exists(context, root)) {
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
                state.particles.push_back(with<rmmr::scene::Interface>::createMeshActor(context, root, rmmr::Pose::from(vec3{crystal.particles[index].position}, HPB{0.0f, 0.0f, 0.0f}), resolved, appearance));
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
            node->pose.position = vec3{particle.position};
            node->pose.rotation = quat{1.0f, 0.0f, 0.0f, 0.0f};
            ++slot;
        }
    }

    void Ui::hideProduction(Writing context) {
        restoreProduction(context);
        for (const auto& hullRef : hullRefs) {
            const auto actor = productionActorOf(context, hullRef.body);
            if (not actor or not with<rmmr::scene::Node>::exists(context, *actor))
                continue;
            rmmr::scene::Node::Actions::setVisible(context, *actor, false);
            hiddenProduction.push_back(*actor);
        }
    }

    void Ui::restoreProduction(Writing context) {
        for (const auto actor : hiddenProduction) {
            if (with<rmmr::scene::Node>::exists(context, actor))
                rmmr::scene::Node::Actions::setVisible(context, actor, true);
        }
        hiddenProduction.clear();
    }

    void Ui::enableHulls(Writing context, rmmr::scene::Root::Id root) {
        disableHulls(context);
        const auto device = first_device(context);
        if (not with<rmmr::scene::Root>::exists(context, root) or not device) {
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
            state.hulls.push_back(with<rmmr::scene::Interface>::createMeshActor(context, root, with<Body>::get(context, crystalId).pose(), resolved, appearance));
            hullRefs.push_back(HullRef{.body = crystalId});
        }
        for (const auto [solidId, _] : context->aspect<rigid::Solid>().items()) {
            const auto& body = with<Body>::get(context, solidId);
            const float radius = body.radius;
            if (radius <= 0.0f)
                continue;
            const auto resolved = rmmr::resource::meshpack::Asset::Resolved{
                .geometry = sphereGeometry,
                .entry = rmmr::resource::geometry::EntryId{0},
                .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = hullMaterial, .textures = {{"albedoMap", "debug02.jpg"}}}}},
                .texpack = hullTexpack,
            };
            const auto solidAppearance = with<rmmr::scene::actor::MeshState>::defaults(rmmr::RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{radius, radius, radius});
            state.hulls.push_back(with<rmmr::scene::Interface>::createMeshActor(context, root, body.pose(), resolved, solidAppearance));
            hullRefs.push_back(HullRef{.body = solidId});
        }
        hideProduction(context);
    }

    void Ui::disableHulls(Writing context) {
        restoreProduction(context);
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
            const bool missingBody = not with<Body>::exists(context, hullRef.body);
            const bool missingActor = not with<rmmr::scene::actor::Mesh>::exists(context, actor);
            if (missingBody or missingActor) {
                if (with<rmmr::scene::actor::Mesh>::exists(context, actor))
                    destroy_actor(context, actor);
                state.hulls.erase(state.hulls.begin() + static_cast<std::ptrdiff_t>(slot));
                hullRefs.erase(hullRefs.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            with<rmmr::scene::Node>::modify(context, actor)->pose = with<Body>::get(context, hullRef.body).pose();
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
                auto thing = with<locality::Thing>::modify_global(context);
                int selected = 4;
                for (int i = 0; i < 9; ++i) {
                    if (thing->timeScale == scales[i]) {
                        selected = i;
                    }
                }
                for (int i = 0; i < 9; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::RadioButton(labels[i], selected == i)) {
                        thing->timeScale = scales[i];
                    }
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Debris");
                {
                    const char* cohortLabels[] = {"Individual", "Families", "Unified"};
                    int cohort = static_cast<int>(Settings::debrisCohort);
                    if (ImGui::Combo("Cohort", &cohort, cohortLabels, 3))
                        Settings::debrisCohort = static_cast<Settings::DebrisCohort>(cohort);
                    if (ImGui::RadioButton("Scrap", Settings::debris == Settings::Debris::scrap))
                        Settings::debris = Settings::Debris::scrap;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Dust", Settings::debris == Settings::Debris::dust))
                        Settings::debris = Settings::Debris::dust;
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Construct");
                ImGui::Checkbox("Collision wounds", &Settings::constructCollisionWounds);
                ImGui::Checkbox("Knot wave", &Settings::knotWave);

                ImGui::Separator();
                ImGui::TextUnformatted("Location");
                if (with<rmmr::scene::Root>::exists(context, system.scene)) {
                    auto root = with<rmmr::scene::Root>::modify(context, system.scene);
                    ImGui::DragFloat3("Gravity", &root->gravity.x, 0.01f, 0.0f, 0.0f, "%.3f m/s²");
                    ImGui::DragFloat("Atmosphere density", &root->atmosphereDensity, 1.0f, 0.0f, 0.0f, "%.0f g/m³");
                    ImGui::DragFloat("Atmosphere temperature", &root->atmosphereTemperature, 0.1f, 0.0f, 0.0f, "%.1f K");
                } else {
                    ImGui::TextDisabled("No scene Root.");
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Debug draw");
                ImGui::Checkbox("Particles", &showParticles);
                ImGui::Checkbox("Collisions", &showHulls);

                ImGui::Separator();
                ImGui::TextUnformatted("Now");
                int particles = 0;
                int hullFaces = 0;
                int boxes = 0;
                int spheres = 0;
                std::unordered_set<Body::Id> cohorts;
                for (const auto [id, _] : context->aspect<rigid::Solid>().items()) {
                    if (not with<Body>::exists(context, id))
                        continue;
                    const auto& body = with<Body>::get(context, id);
                    if (body.radius > 0.0f and body.totalMass > 0.0f)
                        cohorts.insert(body.compound);
                }
                for (const auto [id, _] : context->aspect<rigid::Crystal>().items()) {
                    if (not with<Body>::exists(context, id))
                        continue;
                    const auto& body = with<Body>::get(context, id);
                    if (body.radius > 0.0f and body.totalMass > 0.0f)
                        cohorts.insert(body.compound);
                }
                for (const auto [_, crystal] : context->aspect<rigid::Crystal>().items()) {
                    particles += static_cast<int>(crystal.particles.size());
                    hullFaces += static_cast<int>(crystal.hull.faces.size());
                }
                for (const auto [_, solid] : context->aspect<rigid::Solid>().items()) {
                    if (solid.kind == rigid::Solid::Kind::box)
                        ++boxes;
                    else
                        ++spheres;
                }
                if (ImGui::BeginTable("censusNow", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Kind");
                    ImGui::TableSetupColumn("N", ImGuiTableColumnFlags_WidthFixed, 64.0f);
                    ImGui::TableHeadersRow();
                    censusRow("construct", context->aspect<locality::Construct>().items().size());
                    censusRow("scrap", context->aspect<locality::Scrap>().items().size());
                    censusRow("rock", context->aspect<locality::geo::Rock>().items().size());
                    censusRow("boulder", context->aspect<locality::geo::Boulder>().items().size());
                    censusRow("bullet", context->aspect<locality::Bullet>().items().size());
                    censusRow("flash", context->aspect<locality::Flash>().items().size());
                    censusRow("crystal", context->aspect<rigid::Crystal>().items().size());
                    censusRow("solid", context->aspect<rigid::Solid>().items().size());
                    censusRow("  box", static_cast<std::size_t>(boxes));
                    censusRow("  sphere", static_cast<std::size_t>(spheres));
                    censusRow("ray", context->aspect<rigid::Ray>().items().size());
                    censusRow("cohort", cohorts.size());
                    censusRow("particle", static_cast<std::size_t>(particles));
                    censusRow("hull face", static_cast<std::size_t>(hullFaces));
                    ImGui::EndTable();
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Last collision tick");
                const auto& census = system.collisionCensus();
                ImGui::Text("broad  %d → %d pair tests, sphere %d → obb %d", census.cohorts, census.cohortPairs, census.cohortHits, census.obbHits);
                ImGui::Text("occupants  %d (max %d)  tries %d → candidates %d → contacts %d", census.occupants, census.maxOccupants, census.occupantTries, census.candidates, census.contacts);
                ImGui::Text("rays  %d tries %d, hits %d", census.rays, census.rayTries, census.rayHits);

                ImGui::Separator();
                ImGui::TextUnformatted("Constructs");
                bool any = false;
                for (const auto [id, construct] : context->aspect<locality::Construct>().items()) {
                    if (with<Body>::exists(context, construct.body)) {
                        any = true;
                        break;
                    }
                }
                if (not any) {
                    ImGui::TextDisabled("None.");
                } else if (ImGui::BeginTable("constructs", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 48.0f);
                    ImGui::TableSetupColumn("Mass");
                    ImGui::TableSetupColumn("Speed");
                    ImGui::TableHeadersRow();
                    for (const auto [id, construct] : context->aspect<locality::Construct>().items()) {
                        if (not with<Body>::exists(context, construct.body))
                            continue;
                        const auto& body = with<Body>::get(context, construct.body);
                        float speed = 0.0f;
                        if (with<rigid::Crystal>::exists(context, construct.body))
                            speed = bodyLinearSpeed(with<rigid::Crystal>::get(context, construct.body));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", static_cast<int>(id.raw()));
                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f t", body.totalMass / 1000.0f);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f m/s", speed);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

        if (not open) {
            showParticles = false;
            showHulls = false;
        }

        if (showParticles and not prevShowParticles) {
            enableParticles(context, system.scene);
        } else if (not showParticles and prevShowParticles) {
            disableParticles(context);
        }
        prevShowParticles = showParticles;

        if (showHulls and not prevShowHulls) {
            enableHulls(context, system.scene);
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
