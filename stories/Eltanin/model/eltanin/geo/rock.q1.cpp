#include <eltanin/geo/rock.q1.h>

#include <eltanin/geo/minerals.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/node.q1.h>

#include "mech/semantics/space.h"
#include "physics/system.h"
#include "stones/marchingCubes.h"
#include "stones/crust.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>

namespace eltanin::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr int mixChannels = 16;
        constexpr Mix iceMix = 15;
        constexpr integer iceSphereScale = 3;
        constexpr integer maxScale = 16;

        auto mixDensity(Mix mix) -> float {
            if (mix == 0)
                return 0.0f;
            const auto& table = Mineral::table();
            float density = 0.0f;
            const auto channels = table.size() < static_cast<std::size_t>(mixChannels) ? table.size() : static_cast<std::size_t>(mixChannels);
            for (std::size_t channel = 0; channel < channels; ++channel) {
                const float fill = static_cast<float>((mix >> (channel * 4)) & 0xF) / 15.0f;
                density += fill * table[channel].density;
            }
            return density;
        }

        auto edgeCells(integer scale) -> integer {
            return 1 << scale;
        }

        auto leafMass(const Volume& node) -> float {
            const float meters = static_cast<float>(edgeCells(node.scale)) * mech::space::local::edge2meters;
            return mixDensity(node.mix) * meters * meters * meters;
        }

        auto leafCenterLocal(const Volume& node) -> vec3 {
            const float half = static_cast<float>(edgeCells(node.scale)) * 0.5f;
            return vec3{static_cast<float>(node.origin.x) + half, static_cast<float>(node.origin.y) + half, static_cast<float>(node.origin.z) + half} * mech::space::local::edge2meters;
        }

        auto leafCornerLocal(const Volume& node, ivec3 corner) -> vec3 {
            const integer edge = edgeCells(node.scale);
            return vec3{static_cast<float>(node.origin.x + corner.x * edge), static_cast<float>(node.origin.y + corner.y * edge), static_cast<float>(node.origin.z + corner.z * edge)} * mech::space::local::edge2meters;
        }

        enum class Occupancy { vacuum, solid, mixed };

        auto occupancy(index3 origin, integer scale, float radiusMeters) -> Occupancy {
            const integer edge = edgeCells(scale);
            if (scale == 0) {
                if (glm::length(mech::space::cell::center2local(ivec3{origin.x, origin.y, origin.z})) < radiusMeters)
                    return Occupancy::solid;
                return Occupancy::vacuum;
            }
            const float meters = mech::space::local::edge2meters;
            const vec3 aabbMin = vec3{static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)} * meters;
            const vec3 aabbMax = vec3{static_cast<float>(origin.x + edge), static_cast<float>(origin.y + edge), static_cast<float>(origin.z + edge)} * meters;
            if (glm::length(glm::clamp(vec3{0.0f, 0.0f, 0.0f}, aabbMin, aabbMax)) >= radiusMeters)
                return Occupancy::vacuum;
            const vec3 farthest{
                glm::abs(aabbMin.x) > glm::abs(aabbMax.x) ? aabbMin.x : aabbMax.x,
                glm::abs(aabbMin.y) > glm::abs(aabbMax.y) ? aabbMin.y : aabbMax.y,
                glm::abs(aabbMin.z) > glm::abs(aabbMax.z) ? aabbMin.z : aabbMax.z,
            };
            if (glm::length(farthest) < radiusMeters)
                return Occupancy::solid;
            return Occupancy::mixed;
        }

        auto makeIceNode(index3 origin, integer scale, float radiusMeters) -> optional<Volume> {
            switch (occupancy(origin, scale, radiusMeters)) {
                case Occupancy::vacuum:
                    return {};
                case Occupancy::solid:
                    return Volume{.origin = origin, .scale = scale, .mix = iceMix, .children = {}};
                case Occupancy::mixed:
                    break;
            }
            Volume node{.origin = origin, .scale = scale, .mix = 0, .children = {}};
            const integer childScale = scale - 1;
            const integer half = edgeCells(childScale);
            for (const auto& octant : mech::cube::corners) {
                const index3 childOrigin{origin.x + octant.x * half, origin.y + octant.y * half, origin.z + octant.z * half};
                if (auto child = makeIceNode(childOrigin, childScale, radiusMeters))
                    node.children.push_back(std::move(*child));
            }
            if (node.children.empty())
                return {};
            if (node.children.size() == 8) {
                const Mix mix = node.children[0].mix;
                bool collapse = mix != 0 and node.children[0].children.empty();
                for (const auto& child : node.children) {
                    if (not child.children.empty() or child.mix != mix)
                        collapse = false;
                }
                if (collapse)
                    return Volume{.origin = origin, .scale = scale, .mix = mix, .children = {}};
            }
            return node;
        }

        auto iceSphereVolume() -> Volume {
            const integer half = edgeCells(iceSphereScale) / 2;
            const float radiusMeters = static_cast<float>(half) * mech::space::local::edge2meters;
            const index3 origin{-half, -half, -half};
            if (auto root = makeIceNode(origin, iceSphereScale, radiusMeters))
                return std::move(*root);
            return Volume{.origin = origin, .scale = iceSphereScale, .mix = 0, .children = {}};
        }

        constexpr integer torusScale = 5;
        constexpr float torusMajorCells = 8.0f;
        constexpr float torusMinorCells = 4.0f;

        auto pureMix(int channel) -> Mix {
            return Mix{15} << (channel * 4);
        }

        auto torusInside(vec3 localMeters, float majorMeters, float minorMeters) -> bool {
            const float rho = glm::length(vec2{localMeters.x, localMeters.z});
            return glm::length(vec2{rho - majorMeters, localMeters.y}) < minorMeters;
        }

        auto torusSector(vec3 localMeters) -> int {
            float angle = std::atan2(localMeters.z, localMeters.x);
            if (angle < 0.0f)
                angle += 2.0f * std::numbers::pi_v<float>;
            const int sector = static_cast<int>(angle * static_cast<float>(mixChannels) / (2.0f * std::numbers::pi_v<float>));
            if (sector < 0)
                return 0;
            if (sector >= mixChannels)
                return mixChannels - 1;
            return sector;
        }

        auto torusOccupancy(index3 origin, integer scale, float majorMeters, float minorMeters) -> Occupancy {
            const integer edge = edgeCells(scale);
            const float meters = mech::space::local::edge2meters;
            const vec3 aabbMin = vec3{static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)} * meters;
            const vec3 aabbMax = vec3{static_cast<float>(origin.x + edge), static_cast<float>(origin.y + edge), static_cast<float>(origin.z + edge)} * meters;
            if (aabbMax.y <= -minorMeters or aabbMin.y >= minorMeters)
                return Occupancy::vacuum;
            const vec2 xzMin{aabbMin.x, aabbMin.z};
            const vec2 xzMax{aabbMax.x, aabbMax.z};
            const vec2 xzClosest = glm::clamp(vec2{0.0f, 0.0f}, xzMin, xzMax);
            const float closestRho = glm::length(xzClosest);
            if (closestRho >= majorMeters + minorMeters)
                return Occupancy::vacuum;
            const vec2 xzFarthest{
                glm::abs(aabbMin.x) > glm::abs(aabbMax.x) ? aabbMin.x : aabbMax.x,
                glm::abs(aabbMin.z) > glm::abs(aabbMax.z) ? aabbMin.z : aabbMax.z,
            };
            if (glm::length(xzFarthest) <= majorMeters - minorMeters)
                return Occupancy::vacuum;
            if (scale == 0) {
                if (torusInside(mech::space::cell::center2local(ivec3{origin.x, origin.y, origin.z}), majorMeters, minorMeters))
                    return Occupancy::solid;
                return Occupancy::vacuum;
            }
            return Occupancy::mixed;
        }

        auto makeTorusNode(index3 origin, integer scale, float majorMeters, float minorMeters) -> optional<Volume> {
            switch (torusOccupancy(origin, scale, majorMeters, minorMeters)) {
                case Occupancy::vacuum:
                    return {};
                case Occupancy::solid:
                    if (scale == 0)
                        return Volume{.origin = origin, .scale = scale, .mix = pureMix(torusSector(mech::space::cell::center2local(ivec3{origin.x, origin.y, origin.z}))), .children = {}};
                    break;
                case Occupancy::mixed:
                    break;
            }
            Volume node{.origin = origin, .scale = scale, .mix = 0, .children = {}};
            const integer childScale = scale - 1;
            const integer half = edgeCells(childScale);
            for (const auto& octant : mech::cube::corners) {
                const index3 childOrigin{origin.x + octant.x * half, origin.y + octant.y * half, origin.z + octant.z * half};
                if (auto child = makeTorusNode(childOrigin, childScale, majorMeters, minorMeters))
                    node.children.push_back(std::move(*child));
            }
            if (node.children.empty())
                return {};
            if (node.children.size() == 8) {
                const Mix mix = node.children[0].mix;
                bool collapse = mix != 0 and node.children[0].children.empty();
                for (const auto& child : node.children) {
                    if (not child.children.empty() or child.mix != mix)
                        collapse = false;
                }
                if (collapse)
                    return Volume{.origin = origin, .scale = scale, .mix = mix, .children = {}};
            }
            return node;
        }

        auto paletteTorusVolume() -> Volume {
            const integer half = edgeCells(torusScale) / 2;
            const float meters = mech::space::local::edge2meters;
            const float majorMeters = torusMajorCells * meters;
            const float minorMeters = torusMinorCells * meters;
            const index3 origin{-half, -half, -half};
            if (auto root = makeTorusNode(origin, torusScale, majorMeters, minorMeters))
                return std::move(*root);
            return Volume{.origin = origin, .scale = torusScale, .mix = 0, .children = {}};
        }

        struct Sample {
            vec3 local;
            float mass;
        };

        void collectLeafSamples(const Volume& node, vector<Sample>& samples) {
            if (not node.children.empty()) {
                for (const auto& child : node.children)
                    collectLeafSamples(child, samples);
                return;
            }
            const float mass = leafMass(node);
            if (mass <= 0.0f)
                return;
            const float centerShare = phys::Settings::voxelCenterMassFraction;
            const float cornerShare = (1.0f - centerShare) / 8.0f;
            samples.push_back(Sample{.local = leafCenterLocal(node), .mass = mass * centerShare});
            for (const auto& corner : mech::cube::corners)
                samples.push_back(Sample{.local = leafCornerLocal(node, corner), .mass = mass * cornerShare});
        }

        auto samplesFromVolume(const Volume& volume) -> vector<Sample> {
            vector<Sample> samples;
            collectLeafSamples(volume, samples);
            return samples;
        }

        auto scaleInRange(const Volume& node) -> bool {
            if (node.scale < 0 or node.scale > maxScale)
                return false;
            for (const auto& child : node.children) {
                if (child.scale != node.scale - 1 or not scaleInRange(child))
                    return false;
            }
            return true;
        }

    } // namespace

    auto Rock::Actions::spawn(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose, Volume volume) -> Id {
        if (not scaleInRange(volume))
            return context.refuse("eltanin::geo::Rock::spawn: volume scale out of range");

        const auto samples = samplesFromVolume(volume);
        if (samples.empty())
            return context.refuse("eltanin::geo::Rock::spawn: no mass in volume");

        auto cpu = meshVolume(volume);
        if (cpu.positions.empty())
            return context.refuse("eltanin::geo::Rock::spawn: no surface");

        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        if (not manager)
            return context.refuse("eltanin::geo::Rock::spawn: resource Manager missing");
        if (not with<rmmr::resource::Unit_group>::exists(context, *manager))
            with<rmmr::resource::Unit_group>::extend(context, *manager);
        const auto geometryId = with<rmmr::resource::Unit_group>::addElement(context, *manager, rmmr::resource::Unit::Quantum{.name = rmmr::resource::Unit::Name::from("Eltanin", "rock")});
        with<rmmr::resource::geometry::Asset>::extend(context, geometryId, rmmr::resource::geometry::Asset::Quantum{});
        if (not with<rmmr::resource::geometry::Asset>::install(context, geometryId, device, cpu))
            return context.refuse("eltanin::geo::Rock::spawn: geometry install failed");

        const auto rockMaterial = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "rock"));
        if (not rockMaterial)
            return context.refuse("eltanin::geo::Rock::spawn: rock material missing");
        const auto crust = with<rmmr::resource::Assets>::find<rmmr::resource::texture3array::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "crust"));
        if (not crust)
            return context.refuse("eltanin::geo::Rock::spawn: crust pack missing");
        const auto& runtimes = with<rmmr::resource::Runtimes>::get(context, device);
        if (runtimes.texture3arrays_id_mapping.find(*crust) == runtimes.texture3arrays_id_mapping.end()) {
            if (not with<rmmr::resource::texture3array::Asset>::install(context, *crust, device, generateCrust()))
                return context.refuse("eltanin::geo::Rock::spawn: crust install failed");
        }
        auto meshQuantum = with<rmmr::scene::actor::Mesh>::composeOne(context, geometryId, *rockMaterial, *crust);
        if (not meshQuantum)
            return context.refuse("eltanin::geo::Rock::spawn: mesh compose failed");

        const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f));

        vector<phys::Particle::Id> ids;
        ids.reserve(samples.size());
        vec3 restCom{0.0f, 0.0f, 0.0f};
        float massSum = 0.0f;
        for (const auto& sample : samples) {
            const vec3 world = pose.position + pose.rotation * sample.local;
            ids.push_back(with<phys::Particle>::create(context, phys::Particle::Quantum{.current = world, .prev = world, .mass = sample.mass}));
            restCom += sample.local * sample.mass;
            massSum += sample.mass;
        }
        restCom /= massSum;

        vector<vec3> restCentered;
        restCentered.reserve(samples.size());
        for (const auto& sample : samples)
            restCentered.push_back(sample.local - restCom);

        const auto body = with<phys::Atomic>::create(context, phys::Atomic::Quantum{
            .particles = std::move(ids),
            .rest = phys::Atomic::Rest{.centered = std::move(restCentered), .com = restCom},
            .restored = pose,
        });
        return with<Rock>::create(context, Quantum{.body = body, .actor = actor, .volume = std::move(volume)});
    }

    auto Rock::Actions::spawnIceSphere(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose) -> Id {
        return spawn(context, root, device, pose, iceSphereVolume());
    }

    auto Rock::Actions::spawnPaletteTorus(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose) -> Id {
        return spawn(context, root, device, pose, paletteTorusVolume());
    }

    struct Rock::Internals : Rock::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, rock] : context.proposal.aspect<Rock>().items()) {
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, rock.actor)) { my::remove(context, id); continue; }
                if (not body->restored.near(with<rmmr::scene::Node>::get(context, rock.actor).pose))
                    with<rmmr::scene::Node>::modify(context, rock.actor)->pose = body->restored;
            }
        }
    };

    auto Rock::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Rock, phys::Atomic, &Rock::Quantum::body>{},
            reaction::structural::custody<Rock, rmmr::scene::actor::Mesh, &Rock::Quantum::actor>{},
            reaction::aspect_wide<Rock, phys::Atomic>(&Rock::Internals::followBody),
        };
    }

}
