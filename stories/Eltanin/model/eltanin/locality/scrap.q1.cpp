#include <eltanin/locality/scrap.q1.h>

#include <eltanin/physics/compound.q1.h>
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

#include <span>

namespace eltanin::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float minHalf = 0.25f;
        constexpr float vaporAt = 4.0f;
        constexpr float cutStep = 0.5f;
        constexpr int maxCuts = 3;

        struct Box {
            vec3 center;
            quat rotation;
            vec3 half;
        };

        auto longestAxis(vec3 half) -> int {
            if (half.y > half.x and half.y >= half.z)
                return 1;
            if (half.z > half.x and half.z >= half.y)
                return 2;
            return 0;
        }

        auto cutCount(float cohesion) -> int {
            if (cohesion < -vaporAt)
                return -1;
            if (cohesion >= 0.0f)
                return 0;
            return glm::min(maxCuts, static_cast<int>(-cohesion / cutStep));
        }

        void dichotomy(const Box& box, int cuts, vector<Box>& out) {
            if (cuts <= 0) {
                out.push_back(box);
                return;
            }
            const int axis = longestAxis(box.half);
            if (box.half[axis] < minHalf * 2.0f) {
                out.push_back(box);
                return;
            }
            vec3 unit{0.0f, 0.0f, 0.0f};
            unit[axis] = 1.0f;
            const vec3 along = box.rotation * unit;
            const float h = box.half[axis] * 0.5f;
            Box first = box;
            first.half[axis] = h;
            first.center = box.center - along * h;
            Box second = box;
            second.half[axis] = h;
            second.center = box.center + along * h;
            dichotomy(first, cuts - 1, out);
            dichotomy(second, cuts - 1, out);
        }

    }

    void Scrap::Actions::update(Writing) {
    }

    auto Scrap::Actions::spawn(Writing context, scene::Root::Id root, Pose pose, vec3 halfExtents, float mass, vec3 linear, vec3 omega) -> Id {
        const vec3 half = glm::max(halfExtents, vec3{minHalf, minHalf, minHalf});
        if (mass <= 0.0f)
            return context.refuse("eltanin::locality::Scrap::spawn: mass must be positive");
        const auto kube = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("rmmr", "kube"));
        if (not kube)
            return context.refuse("eltanin::locality::Scrap::spawn: kube geometry missing");
        const auto hull = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "hull"));
        if (not hull)
            return context.refuse("eltanin::locality::Scrap::spawn: hull material missing");
        const auto mech = with<resource::Assets>::find<resource::texpack::Pack>(context, resource::Unit::Name::from("Eltanin", "mech"));
        if (not mech)
            return context.refuse("eltanin::locality::Scrap::spawn: mech texpack missing");
        const auto& geometry = with<resource::geometry::Asset>::get(context, *kube);
        if (geometry.entries.empty())
            return context.refuse("eltanin::locality::Scrap::spawn: kube has no entry");
        const auto& entry = geometry.entries.front();
        umap<resource::geometry::SurfaceId, resource::material::Instance> surfaces;
        for (renderer::Count offset = 0; offset < entry.surfaces.count; ++offset) {
            const auto surface = static_cast<resource::geometry::SurfaceId>(entry.surfaces.first + offset);
            surfaces.emplace(surface, resource::material::Instance{.material = *hull, .textures = {{"albedoMap", "panel_tech_1.bmp"}}});
        }
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, resource::meshpack::Asset::Resolved{.geometry = *kube, .entry = resource::geometry::EntryId{0}, .surfaces = std::move(surfaces), .texpack = *mech});
        if (not meshQuantum)
            return context.refuse("eltanin::locality::Scrap::spawn: mesh compose failed");
        const vec3 scale{half.x * 2.0f, half.y * 2.0f, half.z * 2.0f};
        const auto actor = with<scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, scale));
        const float cohesionZero[] = {0.0f};
        with<scene::actor::Mesh>::writeCohesions(context, actor, std::span<const float>{cohesionZero, 1});
        const float radius = glm::length(half);
        const auto body = with<phys::Body>::create(context, phys::Body::Quantum{.position = dvec3{pose.position}, .orientation = pose.rotation, .totalMass = mass, .radius = radius});
        quat prevOri = pose.rotation;
        const float omegaLen = glm::length(omega);
        if (omegaLen > 1.0e-12f) {
            const quat step = glm::angleAxis(-omegaLen * phys::Particle::dt, omega / omegaLen);
            prevOri = glm::normalize(step * pose.rotation);
        }
        with<phys::rigid::Solid>::extend(context, body, phys::rigid::Solid::Quantum{
            .center = phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = mass, .temperature = 0.0f, .cohesion = 0.0f}, dvec3{pose.position} - dvec3{linear * phys::Particle::dt}, vec3{0.0f, 0.0f, 0.0f}},
            .prevOri = prevOri,
            .forceAngular = vec3{0.0f, 0.0f, 0.0f},
            .kind = phys::rigid::Solid::Kind::box,
            .halfExtents = half,
        });
        with<phys::Compound>::extend(context, body, phys::Compound::Quantum{.members = {}});
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Scrap>::extend(context, thing, Scrap::Quantum{.body = body, .actor = actor});
        return thing;
    }

    void Scrap::Actions::breakOff(Writing context, scene::Root::Id root, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, float cohesion) {
        const int cuts = cutCount(cohesion);
        if (cuts < 0 or mass <= 0.0f)
            return;
        Box seed{.center = worldCenter, .rotation = glm::normalize(worldRot), .half = glm::max(halfExtents, vec3{minHalf, minHalf, minHalf})};
        vector<Box> pieces;
        dichotomy(seed, cuts, pieces);
        if (pieces.empty())
            return;
        const float pieceMass = mass / static_cast<float>(pieces.size());
        const vec3 omega{0.0f, 0.0f, 0.0f};
        for (const Box& piece : pieces) {
            const vec3 offset = piece.center - worldCenter;
            const float offsetLen = glm::length(offset);
            const vec3 pop = offsetLen > 1.0e-5f ? (offset / offsetLen) * 2.0f : vec3{0.0f, 0.0f, 0.0f};
            spawn(context, root, Pose{.position = piece.center, .rotation = piece.rotation}, piece.half, pieceMass, linear + pop, omega);
        }
    }

    struct Scrap::Internals : Scrap::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, scrap] : context.proposal.aspect<Scrap>().items()) {
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, scrap.actor)) { my::remove(context, id); continue; }
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, scrap.actor)->pose = body->pose();
            }
        }
    };

    auto Scrap::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Scrap, rmmr::scene::actor::Mesh, &Scrap::Quantum::actor>{},
            reaction::structural::custody<Scrap, phys::rigid::Solid, &Scrap::Quantum::body>{},
            reaction::aspect_wide<Scrap, phys::Body>(&Scrap::Internals::followBody),
        };
    }

}
