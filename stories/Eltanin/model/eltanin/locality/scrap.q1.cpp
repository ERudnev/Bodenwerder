#include <eltanin/locality/scrap.q1.h>

#include <eltanin/decorations/dust.q1.h>
#include <eltanin/physics/body.q1.h>
#include "physics/settings.h"
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <random>
#include <span>

namespace eltanin::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float minHalf = 0.25f;
        constexpr float vaporAt = 4.0f;
        constexpr float cutStep = 0.5f;
        constexpr int maxCuts = 3;
        constexpr int maxScrapCuts = 2;
        constexpr float bornCohesion = 0.5f;
        constexpr float volumeKeep = 0.25f;
        constexpr int volumeLattice = 3;
        constexpr float heatUploadStep = 10.0f;
        constexpr seconds scrapCool = 20; // thermal Dust remaining²; hotter falls faster

        auto linearOf(const phys::Body::Quantum& body, const phys::rigid::Solid::Quantum& solid) -> vec3 {
            return vec3{(body.position - solid.center.prev) / phys::Settings::fixedStep};
        }

        auto omegaOf(const phys::Body::Quantum& body, const phys::rigid::Solid::Quantum& solid) -> vec3 {
            const float dt = float(phys::Settings::fixedStep);
            const quat qRel = glm::normalize(body.orientation * glm::conjugate(solid.prevOri));
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            const float inertia = 0.4f * body.totalMass * body.radius * body.radius;
            if (inertia > 1.0e-12f)
                omega += (solid.forceAngular / inertia) * dt;
            return omega;
        }

        struct Box {
            vec3 center;
            quat rotation;
            vec3 half;
        };

        struct Chunk {
            Box box;
            float mass;
        };

        auto longestAxis(vec3 half) -> int {
            if (half.y > half.x and half.y >= half.z)
                return 1;
            if (half.z > half.x and half.z >= half.y)
                return 2;
            return 0;
        }

        auto cutsOf(float cohesion) -> int {
            if (cohesion < -vaporAt)
                return -1;
            if (cohesion > 0.0f)
                return 0;
            return glm::min(maxCuts, static_cast<int>(-cohesion / cutStep));
        }

        auto identityPose() -> Pose {
            return Pose{.position = vec3{0.0f, 0.0f, 0.0f}, .rotation = quat{1.0f, 0.0f, 0.0f, 0.0f}};
        }

        auto meshFromBodyOf(const Pose& actorPose, const Pose& bodyPose) -> Pose {
            const quat invBody = glm::conjugate(glm::normalize(bodyPose.rotation));
            return Pose{.position = invBody * (actorPose.position - bodyPose.position), .rotation = glm::normalize(invBody * actorPose.rotation)};
        }

        auto actorPoseOf(const Pose& bodyPose, const Pose& meshFromBody) -> Pose {
            return Pose{.position = bodyPose.position + bodyPose.rotation * meshFromBody.position, .rotation = glm::normalize(bodyPose.rotation * meshFromBody.rotation)};
        }

        auto hangScrap(Writing context, scene::actor::Mesh::Id actor, Pose bodyPose, vec3 half, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature, Pose meshFromBody, Scrap::Lineage lineage, base::maybe<phys::Body::Id> cohort) -> Scrap::Id {
            const float radius = glm::length(half);
            const auto body = phys::createBody(context, phys::Body::Quantum{.position = dvec3{bodyPose.position}, .orientation = bodyPose.rotation, .totalMass = mass, .radius = radius, .compound = phys::Body::Id::please_never_use_this_except_patch_rejection_mechanism()}, cohort);
            quat prevOri = bodyPose.rotation;
            const float omegaLen = glm::length(omega);
            if (omegaLen > 1.0e-12f) {
                const quat step = glm::angleAxis(-omegaLen * float(phys::Settings::fixedStep), omega / omegaLen);
                prevOri = glm::normalize(step * bodyPose.rotation);
            }
            with<phys::rigid::Solid>::extend(context, body, phys::rigid::Solid::Quantum{
                .center = phys::Particle{phys::Matter{.position = dvec3{bodyPose.position}, .mass = mass, .temperature = temperature, .cohesion = cohesion}, dvec3{bodyPose.position} - dvec3{linear * float(phys::Settings::fixedStep)}, vec3{0.0f, 0.0f, 0.0f}},
                .prevOri = prevOri,
                .forceAngular = vec3{0.0f, 0.0f, 0.0f},
                .kind = phys::rigid::Solid::Kind::box,
                .halfExtents = half,
            });
            const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
            with<Scrap>::extend(context, thing, Scrap::Quantum{.body = body, .actor = actor, .gpuKelvin = temperature, .meshFromBody = meshFromBody, .lineage = lineage});
            return thing;
        }

        void paintScrapHeats(Reading context, scene::actor::Mesh::Id actor, phys::Kelvins temperature) {
            if (not with<scene::actor::Mesh>::exists(context, actor))
                return;
            const auto count = with<scene::actor::Mesh>::get(context, actor).instanceCount;
            if (count <= 0)
                return;
            vector<float> heats(static_cast<std::size_t>(count), temperature);
            with<scene::actor::Mesh>::writeHeats(context, actor, std::span<const float>{heats});
        }

        auto cutUneven(const Box& box, Box& first, Box& second) -> float {
            const int axis = longestAxis(box.half);
            vec3 unit{0.0f, 0.0f, 0.0f};
            unit[axis] = 1.0f;
            const vec3 along = box.rotation * unit;
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> minorShare{0.25f, 0.45f};
            std::uniform_real_distribution<float> coin{0.0f, 1.0f};
            const float firstShare = coin(rng) < 0.5f ? minorShare(rng) : 1.0f - minorShare(rng);
            const float full = box.half[axis];
            const float firstH = full * firstShare;
            const float secondH = full - firstH;
            first = box;
            first.half[axis] = firstH;
            first.center = box.center - along * secondH;
            second = box;
            second.half[axis] = secondH;
            second.center = box.center + along * firstH;
            return firstShare;
        }

        void dichotomy(const Box& box, float mass, int cuts, vector<Chunk>& out) {
            if (cuts <= 0) {
                out.push_back(Chunk{.box = box, .mass = mass});
                return;
            }
            const int axis = longestAxis(box.half);
            if (box.half[axis] < minHalf * 2.0f) {
                out.push_back(Chunk{.box = box, .mass = mass});
                return;
            }
            Box first;
            Box second;
            const float firstShare = cutUneven(box, first, second);
            dichotomy(first, mass * firstShare, cuts - 1, out);
            dichotomy(second, mass * (1.0f - firstShare), cuts - 1, out);
        }

        auto volumeKeepRoll() -> bool {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> unit{0.0f, 1.0f};
            return unit(rng) < volumeKeep;
        }

        void burstVolume(Writing context, const Box& seed, float mass, vec3 linear, vec3 omega, phys::Kelvins temperature, base::maybe<phys::Body::Id> cohort) {
            const vec3 childHalf = seed.half / static_cast<float>(volumeLattice);
            const vec3 cell = childHalf * 2.0f;
            const float pieceMass = mass / static_cast<float>(volumeLattice * volumeLattice * volumeLattice);
            for (int ix = -1; ix <= 1; ++ix) {
                for (int iy = -1; iy <= 1; ++iy) {
                    for (int iz = -1; iz <= 1; ++iz) {
                        const vec3 local{static_cast<float>(ix) * cell.x, static_cast<float>(iy) * cell.y, static_cast<float>(iz) * cell.z};
                        const Box piece{.center = seed.center + seed.rotation * local, .rotation = seed.rotation, .half = childHalf};
                        if (volumeKeepRoll())
                            Scrap::Actions::spawn(context, Pose{.position = piece.center, .rotation = piece.rotation}, piece.half, pieceMass, linear, omega, bornCohesion, temperature, Scrap::Lineage::terminal, cohort);
                        else
                            decorations::Dust::Actions::spawnKinetic(context, Pose{.position = piece.center, .rotation = piece.rotation}, piece.half, linear, omega);
                    }
                }
            }
        }

        void spawnKineticPair(Writing context, const Box& box, vec3 linear, vec3 omega) {
            Box first;
            Box second;
            cutUneven(box, first, second);
            decorations::Dust::Actions::spawnKinetic(context, Pose{.position = first.center, .rotation = first.rotation}, first.half, linear, omega);
            decorations::Dust::Actions::spawnKinetic(context, Pose{.position = second.center, .rotation = second.rotation}, second.half, linear, omega);
        }

        void splitScrap(Writing context, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature, base::maybe<phys::Body::Id> cohort) {
            if (temperature >= phys::Settings::Heat::scrapVaporKelvin)
                return;
            const int cuts = cutsOf(cohesion);
            if (cuts < 0 or mass <= 0.0f)
                return;
            Box seed{.center = worldCenter, .rotation = glm::normalize(worldRot), .half = glm::max(halfExtents, vec3{0.08f, 0.08f, 0.08f})};
            const int axis = longestAxis(seed.half);
            const bool canCut = seed.half[axis] >= minHalf * 2.0f;
            if (cuts <= maxScrapCuts and canCut) {
                vector<Chunk> pieces;
                dichotomy(seed, mass, cuts, pieces);
                if (pieces.empty())
                    return;
                for (const Chunk& piece : pieces)
                    Scrap::Actions::spawn(context, Pose{.position = piece.box.center, .rotation = piece.box.rotation}, piece.box.half, piece.mass, linear, omega, bornCohesion, temperature, Scrap::Lineage::common, cohort);
                return;
            }
            if (cuts > maxScrapCuts and canCut) {
                vector<Chunk> quarters;
                dichotomy(seed, mass, maxScrapCuts, quarters);
                for (const Chunk& quarter : quarters)
                    spawnKineticPair(context, quarter.box, linear, omega);
                return;
            }
            spawnKineticPair(context, seed, linear, omega);
        }

    }

    void Scrap::Actions::bindResources(Writing context) {
        if (with<Scrap>::get_global(context).resources)
            return;
        const auto scrap = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "scrap"));
        if (not scrap) {
            context.refuse("eltanin::locality::Scrap::bindResources: scrap geometry missing");
            return;
        }
        const auto wreck = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "wreck"));
        if (not wreck) {
            context.refuse("eltanin::locality::Scrap::bindResources: wreck material missing");
            return;
        }
        const auto mech = with<resource::Assets>::find<resource::texpack::Pack>(context, resource::Unit::Name::from("Eltanin", "mech"));
        if (not mech) {
            context.refuse("eltanin::locality::Scrap::bindResources: mech texpack missing");
            return;
        }
        with<Scrap>::modify_global(context)->resources = Resources{.scrap = *scrap, .wreck = *wreck, .mech = *mech};
    }

    void Scrap::Actions::update(Writing context) {
        vector<Id> living;
        for (auto [id, _] : context->aspect<Scrap>().items())
            living.push_back(id);
        for (const auto id : living) {
            if (not with<Scrap>::exists(context, id) or not with<Thing>::exists(context, id))
                continue;
            if (with<Thing>::get(context, id).bornAt == with<Thing>::get_global(context).now)
                continue;
            const auto& scrap = with<Scrap>::get(context, id);
            if (not with<phys::rigid::Solid>::exists(context, scrap.body) or not with<phys::Body>::exists(context, scrap.body))
                continue;
            const auto& solid = with<phys::rigid::Solid>::get(context, scrap.body);
            const auto& body = with<phys::Body>::get(context, scrap.body);
            if (solid.center.temperature >= phys::Settings::Heat::scrapVaporKelvin) {
                with<Scrap>::kraken(context, id);
                continue;
            }
            const int cuts = cutsOf(solid.center.cohesion);
            if (cuts == 0)
                continue;
            if (cuts < 0) {
                with<Scrap>::kraken(context, id);
                continue;
            }
            const vec3 worldCenter{body.position};
            const vec3 linear = linearOf(body, solid);
            const vec3 omega = omegaOf(body, solid);
            base::maybe<phys::Body::Id> cohort{};
            if (phys::Settings::debrisCohort != phys::Settings::DebrisCohort::individual)
                cohort = body.compound;
            if (scrap.lineage == Lineage::volume) {
                Box seed{.center = worldCenter, .rotation = glm::normalize(body.orientation), .half = glm::max(solid.halfExtents, vec3{0.08f, 0.08f, 0.08f})};
                burstVolume(context, seed, body.totalMass, linear, omega, solid.center.temperature, cohort);
            } else if (scrap.lineage == Lineage::terminal) {
                Box seed{.center = worldCenter, .rotation = glm::normalize(body.orientation), .half = glm::max(solid.halfExtents, vec3{0.08f, 0.08f, 0.08f})};
                spawnKineticPair(context, seed, linear, omega);
            } else {
                splitScrap(context, worldCenter, body.orientation, solid.halfExtents, body.totalMass, linear, omega, solid.center.cohesion, solid.center.temperature, cohort);
            }
            with<Scrap>::kraken(context, id);
        }
    }

    auto Scrap::Actions::spawn(Writing context, Pose pose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature, Lineage lineage, base::maybe<phys::Body::Id> cohort) -> Id {
        const auto scene = with<Thing>::get_global(context).scene;
        const vec3 half = glm::max(halfExtents, vec3{0.08f, 0.08f, 0.08f});
        if (mass <= 0.0f)
            return context.refuse("eltanin::locality::Scrap::spawn: mass must be positive");
        const auto& resources = with<Scrap>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::locality::Scrap::spawn: resources not bound");
        const auto& geometry = with<resource::geometry::Asset>::get(context, resources->scrap);
        if (geometry.entries.empty() or geometry.surfaceCatalogs.empty())
            return context.refuse("eltanin::locality::Scrap::spawn: scrap has no entry");
        const auto& catalog = geometry.surfaceCatalogs.front();
        umap<resource::geometry::SurfaceId, resource::material::Instance> surfaces;
        for (const auto& [_, surface] : catalog)
            surfaces.emplace(surface, resource::material::Instance{.material = resources->wreck, .textures = {{"albedoMap", "wreckage_experimental.jpg"}}});
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, resource::meshpack::Asset::Resolved{.geometry = resources->scrap, .entry = resource::geometry::EntryId{0}, .surfaces = std::move(surfaces), .texpack = resources->mech});
        if (not meshQuantum)
            return context.refuse("eltanin::locality::Scrap::spawn: mesh compose failed");
        const vec3 scale{half.x * 2.0f, half.y * 2.0f, half.z * 2.0f};
        const auto actor = with<scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, scale));
        const float kelvin[] = {temperature};
        with<scene::actor::Mesh>::writeHeats(context, actor, std::span<const float>{kelvin, 1});
        return hangScrap(context, actor, pose, half, mass, linear, omega, cohesion, temperature, identityPose(), lineage, cohort);
    }

    auto Scrap::Actions::spawnMesh(Writing context, Pose actorPose, Pose bodyPose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature, vector<scene::actor::Mesh::Occurrence> occurrences, float latticeStep, Lineage lineage, base::maybe<phys::Body::Id> cohort) -> Id {
        if (mass <= 0.0f)
            return context.refuse("eltanin::locality::Scrap::spawnMesh: mass must be positive");
        if (occurrences.empty())
            return spawn(context, bodyPose, halfExtents, mass, linear, omega, cohesion, temperature, lineage, cohort);
        const auto& resources = with<Scrap>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::locality::Scrap::spawnMesh: resources not bound");
        for (auto& occurrence : occurrences) {
            occurrence.entry.texpack = resources->mech;
            for (auto& [_, instance] : occurrence.entry.surfaces) {
                instance.material = resources->wreck;
                instance.textures = {{"albedoMap", "wreckage_experimental.jpg"}};
            }
        }
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, occurrences);
        if (not meshQuantum)
            return context.refuse("eltanin::locality::Scrap::spawnMesh: mesh compose failed");
        const auto scene = with<Thing>::get_global(context).scene;
        auto look = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        look.latticeStep = latticeStep;
        const auto actor = with<scene::Interface>::createMeshActor(context, scene, actorPose, std::move(*meshQuantum), std::move(look));
        paintScrapHeats(context, actor, temperature);
        const vec3 half = glm::max(halfExtents, vec3{0.01f, 0.01f, 0.01f});
        return hangScrap(context, actor, bodyPose, half, mass, linear, omega, cohesion, temperature, meshFromBodyOf(actorPose, bodyPose), lineage, cohort);
    }

    auto Scrap::Actions::cutCount(float cohesion) -> int {
        return cutsOf(cohesion);
    }

    void Scrap::Actions::breakOff(Writing context, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, float cohesion, phys::Kelvins temperature, base::maybe<phys::Body::Id> cohort) {
        splitScrap(context, worldCenter, worldRot, halfExtents, mass, linear, vec3{0.0f, 0.0f, 0.0f}, cohesion, temperature, cohort);
    }

    void Scrap::Actions::radiate(Stewarding context, seconds dt) {
        if (dt <= 0)
            return;
        const float sky = with<scene::Root>::get(context, with<Thing>::get_global(context).scene).atmosphereTemperature;
        const float remaining = glm::max(0.0f, 1.0f - float(dt) / float(scrapCool));
        const float factor = remaining * remaining;
        auto solids = context.direct<phys::rigid::Solid>();
        for (auto [_, scrap] : context.direct<Scrap>().items) {
            auto* solid = solids.items.find(scrap.body);
            if (not solid or solid->center.temperature <= sky)
                continue;
            solid->center.temperature = glm::max(sky, solid->center.temperature * factor);
        }
    }

    void Scrap::Actions::followBody(Stewarding context) {
        auto nodes = context.direct<scene::Node>();
        auto bodies = context.direct<phys::Body>();
        auto solids = context.direct<phys::rigid::Solid>();
        for (auto [_, scrap] : context.direct<Scrap>().items) {
            auto* node = nodes.items.find(scrap.actor);
            if (not node)
                continue;
            auto* body = bodies.items.find(scrap.body);
            if (not body)
                continue;
            node->pose = actorPoseOf(body->pose(), scrap.meshFromBody);
            auto* solid = solids.items.find(scrap.body);
            if (not solid)
                continue;
            const float kelvin = solid->center.temperature;
            if (glm::abs(kelvin - scrap.gpuKelvin) < heatUploadStep)
                continue;
            scrap.gpuKelvin = kelvin;
            paintScrapHeats(context, scrap.actor, kelvin);
        }
    }

    auto Scrap::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Scrap, rmmr::scene::actor::Mesh, &Scrap::Quantum::actor>{},
            reaction::structural::custody<Scrap, phys::rigid::Solid, &Scrap::Quantum::body>{},
        };
    }

}
