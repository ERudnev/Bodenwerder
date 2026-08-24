#include <eltanin/geo/rock.q1.h>

#include <eltanin/geo/minerals.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/node.q1.h>

#include "mech/semantics/space.h"
#include "geo/stones/marchingCubes.h"
#include "geo/stones/crust.h"
#include "geo/stones/generator.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace eltanin::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr int mixChannels = 16;
        constexpr Mix iceMix = 15;
        constexpr integer iceSphereScale = 3;
        constexpr integer maxScale = 16;
        constexpr float particleSnapMeters = 0.1f;
        constexpr float octreeResolutionRadius = mech::space::local::edge2meters * 2.0f;

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

        void accumulateVolumeMass(const Volume& node, float& mass, vec3& moment) {
            if (not node.children.empty()) {
                for (const auto& child : node.children)
                    accumulateVolumeMass(child, mass, moment);
                return;
            }
            const float leaf = leafMass(node);
            if (leaf <= 0.0f)
                return;
            mass += leaf;
            moment += leafCenterLocal(node) * leaf;
        }

        auto snapKey(vec3 point) -> ivec3 {
            return ivec3{static_cast<int>(std::lround(point.x / particleSnapMeters)), static_cast<int>(std::lround(point.y / particleSnapMeters)), static_cast<int>(std::lround(point.z / particleSnapMeters))};
        }

        auto triangleFace(integer a, integer b, integer c, const vector<vec3>& shape, vec3 inside) -> phys::rigid::Hull::Face {
            const vec3 ab = shape[static_cast<std::size_t>(b)] - shape[static_cast<std::size_t>(a)];
            const vec3 ac = shape[static_cast<std::size_t>(c)] - shape[static_cast<std::size_t>(a)];
            vec3 normal = glm::cross(ab, ac);
            const float mag = glm::length(normal);
            if (mag <= 1.0e-12f)
                return phys::rigid::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}};
            normal /= mag;
            const vec3 centroid = (shape[static_cast<std::size_t>(a)] + shape[static_cast<std::size_t>(b)] + shape[static_cast<std::size_t>(c)]) / 3.0f;
            if (glm::dot(normal, centroid - inside) < 0.0f) {
                std::swap(b, c);
                normal = -normal;
            }
            return phys::rigid::Hull::Face{.points = {a, b, c}, .normal = normal};
        }

        struct SurfaceBody {
            vector<Sample> samples;
            phys::rigid::Hull hull;
        };

        auto surfaceFromMesh(const Volume& volume, const rmmr::resource::builders::geometry::CpuPresentation& cpu) -> SurfaceBody {
            struct Lattice {
                integer x;
                integer y;
                integer z;
                bool operator==(const Lattice&) const = default;
            };
            struct LatticeHash {
                auto operator()(const Lattice& key) const noexcept -> std::size_t {
                    return static_cast<std::size_t>(key.x) * 73856093u ^ static_cast<std::size_t>(key.y) * 19349663u ^ static_cast<std::size_t>(key.z) * 83492791u;
                }
            };
            std::unordered_map<Lattice, integer, LatticeHash> indexOf;
            vector<vec3> rim;
            auto weld = [&](vec3 point) -> integer {
                const ivec3 snapped = snapKey(point);
                const Lattice key{.x = snapped.x, .y = snapped.y, .z = snapped.z};
                const auto found = indexOf.find(key);
                if (found != indexOf.end())
                    return found->second;
                const integer id = static_cast<integer>(rim.size());
                indexOf.emplace(key, id);
                rim.push_back(vec3{static_cast<float>(snapped.x), static_cast<float>(snapped.y), static_cast<float>(snapped.z)} * particleSnapMeters);
                return id;
            };
            vector<std::array<integer, 3>> tris;
            auto emit = [&](std::size_t ia, std::size_t ib, std::size_t ic) {
                if (ia >= cpu.positions.size() or ib >= cpu.positions.size() or ic >= cpu.positions.size())
                    return;
                const integer a = weld(cpu.positions[ia]);
                const integer b = weld(cpu.positions[ib]);
                const integer c = weld(cpu.positions[ic]);
                if (a == b or b == c or c == a)
                    return;
                tris.push_back({a, b, c});
            };
            if (cpu.indices.size() >= 3) {
                for (std::size_t index = 0; index + 2 < cpu.indices.size(); index += 3)
                    emit(static_cast<std::size_t>(cpu.indices[index]), static_cast<std::size_t>(cpu.indices[index + 1]), static_cast<std::size_t>(cpu.indices[index + 2]));
            } else {
                for (std::size_t index = 0; index + 2 < cpu.positions.size(); index += 3)
                    emit(index, index + 1, index + 2);
            }
            float mass = 0.0f;
            vec3 moment{0.0f, 0.0f, 0.0f};
            accumulateVolumeMass(volume, mass, moment);
            SurfaceBody surface{.samples = {}, .hull = phys::rigid::Hull{.faces = {}}};
            if (mass <= 0.0f or rim.empty())
                return surface;
            const vec3 com = moment / mass;
            const float rimMass = (0.5f * mass) / static_cast<float>(rim.size());
            surface.samples.reserve(rim.size() + 1);
            surface.samples.push_back(Sample{.local = com, .mass = 0.5f * mass});
            for (const vec3& local : rim)
                surface.samples.push_back(Sample{.local = local, .mass = rimMass});
            vector<vec3> shape;
            shape.reserve(surface.samples.size());
            for (const Sample& sample : surface.samples)
                shape.push_back(sample.local);
            surface.hull.faces.reserve(tris.size());
            for (const auto& tri : tris) {
                auto face = triangleFace(tri[0] + 1, tri[1] + 1, tri[2] + 1, shape, com);
                if (face.points.size() == 3)
                    surface.hull.faces.push_back(std::move(face));
            }
            return surface;
        }

        auto nearestCohesion(const rmmr::resource::builders::geometry::CpuPresentation& cpu, vec3 local, float fallback) -> float {
            if (cpu.cohesion.empty() or cpu.cohesion.size() != cpu.positions.size())
                return fallback;
            std::size_t best = 0;
            vec3 delta = cpu.positions[0] - local;
            float bestDist = glm::dot(delta, delta);
            for (std::size_t index = 1; index < cpu.positions.size(); ++index) {
                delta = cpu.positions[index] - local;
                const float distance = glm::dot(delta, delta);
                if (distance < bestDist) {
                    bestDist = distance;
                    best = index;
                }
            }
            return cpu.cohesion[best];
        }

        auto sampleRadius(const vector<Sample>& samples) -> float {
            float radius = 0.0f;
            for (const Sample& sample : samples)
                radius = glm::max(radius, glm::length(sample.local));
            return radius;
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

        auto assembleRock(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose, Volume volume, rmmr::resource::builders::geometry::CpuPresentation cpu, vector<Sample> samples, phys::rigid::Hull hull, rmmr::resource::Unit::Name materialName, integer spriteIndex, float temperature, float cohesion, vec3 velocity, vec3 omega) -> Rock::Id {
            if (cpu.positions.empty())
                return context.refuse("eltanin::geo::Rock::spawn: no surface");
            if (samples.empty())
                return context.refuse("eltanin::geo::Rock::spawn: no mass in volume");

            glm::dvec3 massMoment{0.0, 0.0, 0.0};
            double massTotal = 0.0;
            for (const Sample& sample : samples) {
                massMoment += glm::dvec3{sample.local} * static_cast<double>(sample.mass);
                massTotal += static_cast<double>(sample.mass);
            }
            if (massTotal <= 0.0)
                return context.refuse("eltanin::geo::Rock::spawn: mass must be positive");
            const vec3 massCom{massMoment / massTotal};
            const float massSum = static_cast<float>(massTotal);

            vector<float> sampleCohesion;
            sampleCohesion.reserve(samples.size());
            float cohesionSum = 0.0f;
            for (const Sample& sample : samples) {
                const float sampleCohesionValue = nearestCohesion(cpu, sample.local, cohesion);
                sampleCohesion.push_back(sampleCohesionValue);
                cohesionSum += sampleCohesionValue * sample.mass;
            }

            const auto manager = with<rmmr::resource::Manager>::singleton(context);
            const auto geometryId = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = rmmr::resource::Unit::Name::from("Eltanin", "rock")});
            with<rmmr::resource::geometry::Asset>::extend(context, geometryId, rmmr::resource::geometry::Asset::Quantum{});
            if (not with<rmmr::resource::geometry::Asset>::install(context, geometryId, device, cpu))
                return context.refuse("eltanin::geo::Rock::spawn: geometry install failed");

            const auto rockMaterial = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, materialName);
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
            meshQuantum->spriteIndex = spriteIndex;

            auto meshState = with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
            meshState.patternScale = glm::max(0.5f, sampleRadius(samples) * 2.0f);
            meshState.heat = vec2{temperature, cohesionSum / massSum};
            const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), meshState);

            vector<phys::Particle> particles;
            particles.reserve(samples.size());
            vector<vec3> shape;
            shape.reserve(samples.size());
            for (std::size_t index = 0; index < samples.size(); ++index) {
                const Sample& sample = samples[index];
                const vec3 world = pose.position + pose.rotation * sample.local;
                const vec3 spin = glm::cross(omega, pose.rotation * (sample.local - massCom));
                particles.push_back(phys::Particle{phys::Matter{.position = world, .mass = sample.mass, .temperature = temperature, .cohesion = sampleCohesion[index]}, world - (velocity + spin) * phys::Particle::dt, vec3{0.0f, 0.0f, 0.0f}});
                shape.push_back(sample.local);
            }

            const auto body = with<phys::Body>::create(context, phys::rigid::restoredBody(pose, particles, shape));
            with<phys::rigid::Crystal>::extend(context, body, phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = massCom,
                .hull = std::move(hull),
            });
            return with<Rock>::create(context, Rock::Quantum{.body = body, .actor = actor, .volume = std::move(volume)});
        }

    } // namespace

    auto Rock::Actions::spawn(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose, Volume volume, vec3 velocity, vec3 omega) -> Id {
        if (not scaleInRange(volume))
            return context.refuse("eltanin::geo::Rock::spawn: volume scale out of range");
        auto cpu = meshVolume(volume);
        auto surface = surfaceFromMesh(volume, cpu);
        return assembleRock(context, root, device, pose, std::move(volume), std::move(cpu), std::move(surface.samples), std::move(surface.hull), rmmr::resource::Unit::Name::from("Eltanin", "rock"), 0, 0.0f, 0.0f, velocity, omega);
    }

    auto Rock::Actions::spawnGenerated(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose, GeneralizedRecipe recipe, vec3 velocity, vec3 omega) -> Id {
        if (recipe.radius <= 0.0f)
            return context.refuse("eltanin::geo::Rock::spawnGenerated: radius must be positive");
        if (recipe.mix == 0)
            return context.refuse("eltanin::geo::Rock::spawnGenerated: mix is vacuum");
        if (recipe.radius <= octreeResolutionRadius)
            return context.refuse("eltanin::geo::Rock::spawnGenerated: radius too small for Crystal; use Boulder");
        auto volume = generateRockVolume(recipe);
        if (not scaleInRange(volume))
            return context.refuse("eltanin::geo::Rock::spawnGenerated: volume scale out of range");
        auto cpu = meshVolume(volume);
        auto surface = surfaceFromMesh(volume, cpu);
        return assembleRock(context, root, device, pose, std::move(volume), std::move(cpu), std::move(surface.samples), std::move(surface.hull), rmmr::resource::Unit::Name::from("Eltanin", "rock"), 0, 0.0f, 0.0f, velocity, omega);
    }

    auto Rock::Actions::spawnIceSphere(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose) -> Id {
        return spawn(context, root, device, pose, iceSphereVolume(), vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
    }

    auto Rock::Actions::spawnPaletteTorus(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose) -> Id {
        return spawn(context, root, device, pose, paletteTorusVolume(), vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
    }

    auto Rock::Actions::spawnLavaBrick(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose) -> Id {
        auto volume = generateLavaBrickVolume();
        if (not scaleInRange(volume))
            return context.refuse("eltanin::geo::Rock::spawnLavaBrick: volume scale out of range");
        auto cpu = meshVolume(volume);
        applyLavaBrickHeat(cpu);
        auto surface = surfaceFromMesh(volume, cpu);
        return assembleRock(context, root, device, pose, std::move(volume), std::move(cpu), std::move(surface.samples), std::move(surface.hull), rmmr::resource::Unit::Name::from("Eltanin", "rock"), 0, 80.0f, 1.0f, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
    }

    auto Rock::Actions::spawnIceBlob(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose) -> Id {
        auto volume = generateIceBlobVolume();
        if (not scaleInRange(volume))
            return context.refuse("eltanin::geo::Rock::spawnIceBlob: volume scale out of range");
        auto cpu = meshVolume(volume);
        applyIceBlobSinter(cpu);
        auto surface = surfaceFromMesh(volume, cpu);
        return assembleRock(context, root, device, pose, std::move(volume), std::move(cpu), std::move(surface.samples), std::move(surface.hull), rmmr::resource::Unit::Name::from("Eltanin", "rock"), 0, 80.0f, 0.0f, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
    }

    void Rock::Actions::radiate(Stewarding, float dt) {
        if (dt <= 0.0f)
            return;
        // Volume rocks: thermal / conduction later (destruction path). Boulder handles small-body radiate.
    }

    auto GeneralizedRecipe::homogenous(Mineral::Index mineral) -> Mix {
        if (mineral < 0 or mineral >= mixChannels)
            return 0;
        return Mix{15} << (mineral * 4);
    }

    struct Rock::Internals : Rock::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, rock] : context.proposal.aspect<Rock>().items()) {
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, rock.actor)) { my::remove(context, id); continue; }
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, rock.actor)->pose = body->pose();
            }
        }
    };

    auto Rock::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Rock, rmmr::scene::actor::Mesh, &Rock::Quantum::actor>{},
            reaction::aspect_wide<Rock, phys::Body>(&Rock::Internals::followBody),
        };
    }

}
