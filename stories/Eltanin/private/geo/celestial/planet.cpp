#include "geo/celestial/planet.h"
#include "geo/celestial/horizon.h"
#include "geo/celestial/generator.h"
#include "physics/settings.h"

#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/patchGrid.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/common.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;
    using locality::Thing;

    namespace {

        auto heightNormal(const Planet& planet, vec3 dir) -> dvec3 {
            const float len = glm::length(dir);
            if (len < 1.0e-6f)
                return dvec3{0.0, 1.0, 0.0};
            dir /= len;
            vec3 tangentU = glm::cross(vec3{0.0f, 1.0f, 0.0f}, dir);
            if (glm::dot(tangentU, tangentU) < 1.0e-8f)
                tangentU = glm::cross(vec3{1.0f, 0.0f, 0.0f}, dir);
            tangentU = glm::normalize(tangentU);
            const vec3 tangentV = glm::cross(dir, tangentU);
            const float eps = 1.0f / float(std::max(planet.heights.pack.edgeSegments(), integer{1}));
            auto surface = [&](vec3 sample) -> dvec3 {
                sample = glm::normalize(sample);
                return dvec3{sample} * planet.height(sample);
            };
            dvec3 normal = glm::cross(surface(dir + tangentU * eps) - surface(dir - tangentU * eps), surface(dir + tangentV * eps) - surface(dir - tangentV * eps));
            const double mag = glm::length(normal);
            if (mag < 1.0e-12)
                return dvec3{dir};
            normal /= mag;
            if (glm::dot(normal, dvec3{dir}) < 0.0)
                return -normal;
            return normal;
        }

        constexpr integer patchCells = 32;
        constexpr float lodK = 0.18f;

        auto firstLodDistance(const geo::IcosaPack& pack, float radius) -> float {
            const double arc = std::max(0.0, double(radius)) * std::acos(1.0 / std::sqrt(5.0));
            const float texelMeters = float(arc / double(std::max(pack.edgeSegments(), integer{1})));
            return std::sqrt(2.0f) * float(patchCells) * std::max(texelMeters, 1.0e-4f) / lodK;
        }

        auto rootStepOf(integer segments) -> integer {
            integer step = 1;
            while (patchCells * step < segments)
                step *= 2;
            return step;
        }

        auto heightArray(const Planet& planet) -> vector<std::int16_t> {
            const auto& pack = planet.heights.pack;
            const integer span = pack.edgeVertices();
            const integer layers = geo::IcosaPack::diamondCount;
            vector<std::int16_t> packed(static_cast<std::size_t>(layers) * static_cast<std::size_t>(span) * static_cast<std::size_t>(span), std::int16_t{0});
            const integer count = pack.storedCount();
            for (integer index = 0; index < count; ++index) {
                const auto slot = pack.slotOf(index);
                packed[static_cast<std::size_t>((slot.diamond * span + slot.iv) * span + slot.iu)] = planet.heights.at(slot);
            }
            return packed;
        }

        auto coverArray(const Planet& planet) -> vector<std::uint8_t> {
            const auto& pack = planet.covers.pack;
            const integer span = pack.edgeVertices();
            const integer layers = geo::IcosaPack::diamondCount;
            const integer count = pack.storedCount();
            vector<std::uint8_t> packed(static_cast<std::size_t>(layers) * static_cast<std::size_t>(span) * static_cast<std::size_t>(span) * 2u, std::uint8_t{0});
            for (integer index = 0; index < count; ++index) {
                const auto slot = pack.slotOf(index);
                const std::uint16_t packedCover = planet.covers.at(slot);
                const std::size_t offset = static_cast<std::size_t>((slot.diamond * span + slot.iv) * span + slot.iu) * 2u;
                packed[offset] = static_cast<std::uint8_t>(packedCover & 255u);
                packed[offset + 1] = static_cast<std::uint8_t>((packedCover >> 8) & 255u);
            }
            return packed;
        }

        auto farNormalArray(const Planet& planet) -> vector<std::uint8_t> {
            const auto& pack = planet.farNormal.pack;
            const integer span = pack.edgeVertices();
            const integer layers = geo::IcosaPack::diamondCount;
            vector<std::uint8_t> packed(static_cast<std::size_t>(layers) * static_cast<std::size_t>(span) * static_cast<std::size_t>(span) * 4u, std::uint8_t{0});
            for (integer index = 0; index < pack.storedCount(); ++index) {
                const auto slot = pack.slotOf(index);
                const vec3 encoded = glm::clamp(planet.farNormal.at(slot) * 0.5f + 0.5f, vec3{0.0f}, vec3{1.0f});
                const std::size_t offset = static_cast<std::size_t>((slot.diamond * span + slot.iv) * span + slot.iu) * 4u;
                packed[offset] = static_cast<std::uint8_t>(std::lround(encoded.x * 255.0f));
                packed[offset + 1] = static_cast<std::uint8_t>(std::lround(encoded.y * 255.0f));
                packed[offset + 2] = static_cast<std::uint8_t>(std::lround(encoded.z * 255.0f));
                packed[offset + 3] = std::uint8_t{255};
            }
            return packed;
        }

        auto farAlbedoArray(const Planet& planet) -> vector<std::uint8_t> {
            const auto& pack = planet.farAlbedo.pack;
            const integer span = pack.edgeVertices();
            const integer layers = geo::IcosaPack::diamondCount;
            vector<std::uint8_t> packed(static_cast<std::size_t>(layers) * static_cast<std::size_t>(span) * static_cast<std::size_t>(span) * 4u, std::uint8_t{0});
            for (integer index = 0; index < pack.storedCount(); ++index) {
                const auto slot = pack.slotOf(index);
                const vec4 color = glm::clamp(planet.farAlbedo.at(slot), vec4{0.0f}, vec4{1.0f});
                const std::size_t offset = static_cast<std::size_t>((slot.diamond * span + slot.iv) * span + slot.iu) * 4u;
                packed[offset] = static_cast<std::uint8_t>(std::lround(color.x * 255.0f));
                packed[offset + 1] = static_cast<std::uint8_t>(std::lround(color.y * 255.0f));
                packed[offset + 2] = static_cast<std::uint8_t>(std::lround(color.z * 255.0f));
                packed[offset + 3] = static_cast<std::uint8_t>(std::lround(color.w * 255.0f));
            }
            return packed;
        }

        auto icosaShell() -> scene::actor::PatchGrid::Shell {
            scene::actor::PatchGrid::Shell shell;
            const auto& vertices = geo::IcosaPack::vertices();
            const auto& diamonds = geo::IcosaPack::diamonds();
            for (integer index = 0; index < geo::IcosaPack::shellCount; ++index)
                shell.vertices[static_cast<std::size_t>(index)] = vec4{vertices[static_cast<std::size_t>(index)], 0.0f};
            for (integer index = 0; index < geo::IcosaPack::diamondCount; ++index) {
                const auto& diamond = diamonds[static_cast<std::size_t>(index)];
                shell.diamonds[static_cast<std::size_t>(index)] = glm::ivec4{static_cast<int>(diamond.top), static_cast<int>(diamond.right), static_cast<int>(diamond.bottom), static_cast<int>(diamond.left)};
            }
            return shell;
        }

        auto coarsePatches(const geo::IcosaPack& pack) -> vector<scene::actor::PatchGrid::Patch> {
            vector<scene::actor::PatchGrid::Patch> patches;
            const integer segments = pack.edgeSegments();
            const integer step = rootStepOf(segments);
            for (integer diamond = 0; diamond < geo::IcosaPack::diamondCount; ++diamond)
                patches.push_back(scene::actor::PatchGrid::Patch{.diamond = diamond, .originU = 0, .originV = 0, .step = step, .stepNegU = step, .stepPosU = step, .stepNegV = step, .stepPosV = step});
            return patches;
        }

        auto lodPatches(const geo::IcosaPack& pack, float radius, vec3 localCamera) -> vector<scene::actor::PatchGrid::Patch> {
            const integer segments = pack.edgeSegments();
            const integer rootStep = rootStepOf(segments);
            const double arc = std::max(0.0, double(radius)) * std::acos(1.0 / std::sqrt(5.0));
            const float texelMeters = float(arc / double(std::max(segments, integer{1})));
            const float metresPerCell = float(patchCells) * std::max(texelMeters, 1.0e-4f);
            auto wantStepAt = [&](vec3 surface) -> integer {
                const float dist = std::max(glm::length(localCamera - surface), metresPerCell);
                const float raw = dist * lodK / metresPerCell;
                integer want = static_cast<integer>(std::lround(std::exp2(std::round(std::log2(std::max(raw, 1.0f))))));
                if (want < 1)
                    want = 1;
                if (want > rootStep)
                    want = rootStep;
                return want;
            };
            struct Tile {
                integer diamond;
                integer originU;
                integer originV;
                integer step;
            };
            vector<Tile> tiles;
            auto visit = [&](auto& self, integer diamond, integer originU, integer originV, integer step) -> void {
                if (originU >= segments or originV >= segments)
                    return;
                const integer midU = std::clamp(originU + (patchCells * step) / 2, integer{0}, segments);
                const integer midV = std::clamp(originV + (patchCells * step) / 2, integer{0}, segments);
                const vec3 center = pack.direction(geo::IcosaPack::Slot{.diamond = diamond, .iu = midU, .iv = midV}) * radius;
                const integer want = wantStepAt(center);
                if (step > want and step > 1) {
                    const integer half = step / 2;
                    const integer span = patchCells * half;
                    self(self, diamond, originU, originV, half);
                    self(self, diamond, originU + span, originV, half);
                    self(self, diamond, originU, originV + span, half);
                    self(self, diamond, originU + span, originV + span, half);
                    return;
                }
                tiles.push_back(Tile{.diamond = diamond, .originU = originU, .originV = originV, .step = step});
            };
            for (integer diamond = 0; diamond < geo::IcosaPack::diamondCount; ++diamond)
                visit(visit, diamond, 0, 0, rootStep);
            const integer buckets = std::max((segments + patchCells - 1) / patchCells, integer{1});
            auto paint = [&](vector<integer>& stepMap) {
                std::fill(stepMap.begin(), stepMap.end(), rootStep);
                for (const auto& tile : tiles) {
                    const integer bu0 = tile.originU / patchCells;
                    const integer bv0 = tile.originV / patchCells;
                    for (integer bv = 0; bv < tile.step; ++bv) {
                        for (integer bu = 0; bu < tile.step; ++bu) {
                            const integer u = bu0 + bu;
                            const integer v = bv0 + bv;
                            if (u >= 0 and u < buckets and v >= 0 and v < buckets)
                                stepMap[static_cast<std::size_t>((tile.diamond * buckets + v) * buckets + u)] = tile.step;
                        }
                    }
                }
            };
            vector<integer> stepMap(static_cast<std::size_t>(geo::IcosaPack::diamondCount * buckets * buckets), rootStep);
            for (integer pass = 0; pass < 8; ++pass) {
                paint(stepMap);
                vector<Tile> next;
                bool split = false;
                for (const auto& tile : tiles) {
                    const integer bu0 = tile.originU / patchCells;
                    const integer bv0 = tile.originV / patchCells;
                    const integer mid = std::max(tile.step / 2, integer{0});
                    auto sample = [&](integer bu, integer bv) -> integer {
                        if (bu < 0 or bv < 0 or bu >= buckets or bv >= buckets)
                            return tile.step;
                        return stepMap[static_cast<std::size_t>((tile.diamond * buckets + bv) * buckets + bu)];
                    };
                    const integer nmin = std::min(std::min(sample(bu0 - 1, bv0 + mid), sample(bu0 + tile.step, bv0 + mid)), std::min(sample(bu0 + mid, bv0 - 1), sample(bu0 + mid, bv0 + tile.step)));
                    if (tile.step > nmin * 2 and tile.step > 1) {
                        split = true;
                        const integer half = tile.step / 2;
                        const integer span = patchCells * half;
                        auto push = [&](integer ou, integer ov) {
                            if (ou < segments and ov < segments)
                                next.push_back(Tile{.diamond = tile.diamond, .originU = ou, .originV = ov, .step = half});
                        };
                        push(tile.originU, tile.originV);
                        push(tile.originU + span, tile.originV);
                        push(tile.originU, tile.originV + span);
                        push(tile.originU + span, tile.originV + span);
                    } else {
                        next.push_back(tile);
                    }
                }
                tiles = std::move(next);
                if (not split)
                    break;
            }
            paint(stepMap);
            vector<scene::actor::PatchGrid::Patch> patches;
            patches.reserve(tiles.size());
            for (const auto& tile : tiles) {
                const integer bu0 = tile.originU / patchCells;
                const integer bv0 = tile.originV / patchCells;
                const integer mid = std::max(tile.step / 2, integer{0});
                auto sample = [&](integer bu, integer bv) -> integer {
                    if (bu < 0 or bv < 0 or bu >= buckets or bv >= buckets)
                        return tile.step;
                    return stepMap[static_cast<std::size_t>((tile.diamond * buckets + bv) * buckets + bu)];
                };
                patches.push_back(scene::actor::PatchGrid::Patch{.diamond = tile.diamond, .originU = tile.originU, .originV = tile.originV, .step = tile.step, .stepNegU = sample(bu0 - 1, bv0 + mid), .stepPosU = sample(bu0 + tile.step, bv0 + mid), .stepNegV = sample(bu0 + mid, bv0 - 1), .stepPosV = sample(bu0 + mid, bv0 + tile.step)});
            }
            return patches;
        }

        auto orientationOf(const Passport& passport) -> quat {
            const vec3 axis = glm::length(passport.spin.axis) > 1.0e-6f ? glm::normalize(passport.spin.axis) : vec3{0.0f, 1.0f, 0.0f};
            const float alignment = glm::dot(vec3{0.0f, 1.0f, 0.0f}, axis);
            if (alignment < -0.99999f)
                return glm::angleAxis(std::numbers::pi_v<float>, vec3{1.0f, 0.0f, 0.0f});
            return glm::normalize(quat{1.0f + alignment, axis.z, 0.0f, -axis.x});
        }

        void bindAtmosphereMesh(scene::actor::MeshState::Quantum& mesh, const Planet& planet) {
            mesh.albedo = planet.runtime.atmosphere.day;
            mesh.opacity = planet.runtime.atmosphere.seaDensity;
            mesh.heat = vec2{planet.passport.radius, planet.runtime.atmosphere.outerRadius};
            mesh.scale = vec3{planet.runtime.atmosphere.outerRadius * 1.08f};
            mesh.latticeStep = planet.runtime.atmosphere.zenithTau;
            mesh.patternScale = geo::Horizon::locality;
        }

        auto spawnAtmosphere(Writing context, Pose pose, const Planet& planet) -> base::maybe<scene::actor::Mesh::Id> {
            if (planet.runtime.atmosphere.seaDensity <= 0.0f or planet.runtime.atmosphere.outerRadius <= planet.passport.radius)
                return {};
            const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "atmosphere"));
            if (not material) {
                context.refuse("eltanin::planet::Planet::place: atmosphere material missing");
                return {};
            }
            const auto sphere = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "atmosphereSphere"));
            if (not sphere) {
                context.refuse("eltanin::planet::Planet::place: atmosphereSphere geometry missing");
                return {};
            }
            auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, *sphere, *material);
            if (not meshQuantum) {
                context.refuse("eltanin::planet::Planet::place: atmosphere mesh compose failed");
                return {};
            }
            auto meshState = with<scene::actor::MeshState>::defaults(planet.runtime.atmosphere.day, planet.runtime.atmosphere.seaDensity, vec3{1.0f});
            bindAtmosphereMesh(meshState, planet);
            return with<scene::Interface>::createMeshActor(context, with<Thing>::get_global(context).scene, pose, std::move(*meshQuantum), meshState);
        }

        auto toLocal(const phys::Body::Quantum& body, dvec3 worldPos) -> dvec3 {
            return glm::inverse(body.orientation) * (worldPos - body.position);
        }

        auto spinRate(const Planet& planet) -> double {
            return planet.passport.spin.period > 0.0f ? 2.0 * std::numbers::pi / double(planet.passport.spin.period) : 0.0;
        }

        auto spinAxis(const Planet& planet) -> dvec3 {
            return glm::length(planet.passport.spin.axis) > 1.0e-6f ? glm::normalize(dvec3{planet.passport.spin.axis}) : dvec3{0.0, 1.0, 0.0};
        }

        auto wellQuantum(const Planet& planet, Pose pose) -> phys::Body::Quantum {
            const float radius = planet.passport.radius;
            return phys::Body::Quantum{
                .position = dvec3{pose.position},
                .orientation = dquat{orientationOf(planet.passport)},
                .totalMass = static_cast<float>(planet.passport.mass),
                .radius = radius + planet.runtime.reliefAmplitude,
                .compound = phys::Body::Id::please_never_use_this_except_patch_rejection_mechanism(),
            };
        }

        void bindWell(phys::Body::Quantum& body, const Planet& planet, Pose pose) {
            const auto next = wellQuantum(planet, pose);
            body.position = next.position;
            body.orientation = next.orientation;
            body.totalMass = next.totalMass;
            body.radius = next.radius;
        }

    }

    auto Planet::recommendedDetail(float radius, float edge) -> Detail {
        constexpr integer maxEdgeSegments = 2048; // GPU height/cover field; N = edgeBase * 2^t
        const integer cap = maxEdgeSegments;
        const float want = edge > 0.0f ? edge : constructionEdge;
        const double arc = std::max(0.0, double(radius)) * std::acos(1.0 / std::sqrt(5.0));
        integer segments = want > 0.0f ? static_cast<integer>(std::lround(arc / double(want))) : cap;
        if (segments < 1)
            segments = 1;
        if (segments > cap)
            segments = cap;
        integer tessellation = 0;
        integer edgeBase = segments;
        while (edgeBase % 2 == 0) {
            edgeBase /= 2;
            tessellation += 1;
        }
        return Detail{.edgeBase = edgeBase, .tessellation = tessellation};
    }

    Planet::Planet(Passport passport, Detail detail)
        : passport{passport}
        , runtime{.surfaceAcceleration = 0.0f, .reliefAmplitude = 0.0f, .atmosphere = {.outerRadius = passport.radius, .seaDensity = 0.0f, .kerman = 1.0f, .zenithTau = 0.0f, .day = RGB{0.0f, 0.0f, 0.0f}}}
        , spinOmega{spinAxis(*this) * (passport.spin.period > 0.0f ? 2.0 * std::numbers::pi / double(passport.spin.period) : 0.0)}
        , well{}
        , heights{geo::IcosaPack{.edgeBase = detail.edgeBase, .tessellation = detail.tessellation}, std::int16_t{0}}
        , covers{heights.pack, std::uint16_t{0}}
        , farAlbedo{geo::IcosaPack{.edgeBase = std::max(heights.pack.edgeSegments() / 2, integer{1}), .tessellation = 0}, vec4{0.0f}}
        , farNormal{farAlbedo.pack, vec3{0.0f, 1.0f, 0.0f}}
        , shell{}
        , atmosphere{} {
        Generator::generate(*this);
    }

    void Planet::place(Writing context, system::Device::Id device, Pose pose) {
        pose.rotation = orientationOf(passport);
        const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "planet"));
        if (not material) {
            context.refuse("eltanin::planet::Planet::place: planet material missing");
            return;
        }
        const auto grid = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "patchGrid"));
        if (not grid) {
            context.refuse("eltanin::planet::Planet::place: patchGrid geometry missing");
            return;
        }
        const auto facies = with<resource::Assets>::find<resource::texpack::Pack>(context, resource::Unit::Name::from("Eltanin", "facies"));
        if (not facies) {
            context.refuse("eltanin::planet::Planet::place: facies texpack missing");
            return;
        }
        const auto manager = with<resource::Manager>::singleton(context);
        const integer span = heights.pack.edgeVertices();
        const integer layers = geo::IcosaPack::diamondCount;
        const auto heightPixels = heightArray(*this);
        const auto coverPixels = coverArray(*this);
        const auto farPixels = farAlbedoArray(*this);
        const auto farNormalPixels = farNormalArray(*this);
        const auto heightId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-height")});
        with<resource::texture::Asset>::extend(context, heightId, resource::texture::Asset::Quantum{});
        const auto heightBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(heightPixels.data()), heightPixels.size() * sizeof(std::int16_t));
        if (not with<resource::texture::Asset>::install(context, heightId, device, resource::texture::Asset::Format::r16Snorm, resource::texture::Asset::Sampling::nearest, index2{.x = span, .y = span}, layers, 1, heightBytes)) {
            context.refuse("eltanin::planet::Planet::place: height atlas install failed");
            return;
        }
        const auto coverId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-cover")});
        with<resource::texture::Asset>::extend(context, coverId, resource::texture::Asset::Quantum{});
        const auto coverBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(coverPixels.data()), coverPixels.size());
        if (not with<resource::texture::Asset>::install(context, coverId, device, resource::texture::Asset::Format::rg8, resource::texture::Asset::Sampling::nearest, index2{.x = span, .y = span}, layers, 1, coverBytes)) {
            context.refuse("eltanin::planet::Planet::place: cover atlas install failed");
            return;
        }
        const integer farSpan = farAlbedo.pack.edgeVertices();
        const auto farAlbedoId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-far-albedo")});
        with<resource::texture::Asset>::extend(context, farAlbedoId, resource::texture::Asset::Quantum{});
        const auto farBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(farPixels.data()), farPixels.size());
        if (not with<resource::texture::Asset>::install(context, farAlbedoId, device, resource::texture::Asset::Format::rgba8, resource::texture::Asset::Sampling::linear, index2{.x = farSpan, .y = farSpan}, layers, 1, farBytes)) {
            context.refuse("eltanin::planet::Planet::place: far albedo atlas install failed");
            return;
        }
        const auto farNormalId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-far-normal")});
        with<resource::texture::Asset>::extend(context, farNormalId, resource::texture::Asset::Quantum{});
        const auto farNormalBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(farNormalPixels.data()), farNormalPixels.size());
        if (not with<resource::texture::Asset>::install(context, farNormalId, device, resource::texture::Asset::Format::rgba8, resource::texture::Asset::Sampling::linear, index2{.x = farSpan, .y = farSpan}, layers, 1, farNormalBytes)) {
            context.refuse("eltanin::planet::Planet::place: far normal atlas install failed");
            return;
        }
        const auto patches = coarsePatches(heights.pack);
        auto gridQuantum = with<scene::actor::PatchGrid>::compose(context, *grid, *material, *facies, heightId, coverId, farAlbedoId, farNormalId, icosaShell(), patches, passport.radius, runtime.reliefAmplitude, firstLodDistance(heights.pack, passport.radius), heights.pack.edgeVertices(), patchCells);
        if (not gridQuantum) {
            context.refuse("eltanin::planet::Planet::place: patch grid compose failed");
            return;
        }
        shell = with<scene::Interface>::createPatchGridActor(context, with<Thing>::get_global(context).scene, pose, std::move(*gridQuantum));
        with<scene::Root>::modify(context, with<Thing>::get_global(context).scene)->atmosphereDensity = runtime.atmosphere.seaDensity;
        with<scene::Root>::modify(context, with<Thing>::get_global(context).scene)->atmosphereKerman = runtime.atmosphere.kerman;
        atmosphere = spawnAtmosphere(context, pose, *this);
        if (not well)
            well = phys::createBody(context, wellQuantum(*this, pose), {});
        else
            bindWell(*with<phys::Body>::modify(context, *well), *this, pose);
        sync(context);
    }

    void Planet::update(Writing context, Pos camera) {
        if (not well or not with<phys::Body>::exists(context, *well))
            return;
        const auto& body = with<phys::Body>::get(context, *well);
        if (shell and with<scene::actor::PatchGrid>::exists(context, *shell)) {
            const vec3 localCamera{toLocal(body, dvec3{camera})};
            const auto patches = lodPatches(heights.pack, passport.radius, localCamera);
            with<scene::actor::PatchGrid>::setPatches(context, *shell, patches);
        }
        sync(context);
    }

    void Planet::advancePhysics(Writing context, seconds dt) {
        if (not well or not with<phys::Body>::exists(context, *well))
            return;
        auto body = with<phys::Body>::modify(context, *well);
        const double rate = spinRate(*this);
        spinOmega = spinAxis(*this) * rate;
        if (rate != 0.0)
            body->orientation = glm::normalize(glm::angleAxis(rate * double(dt), glm::normalize(spinOmega)) * body->orientation);
    }

    void Planet::sync(Writing context) {
        if (not well or not with<phys::Body>::exists(context, *well))
            return;
        const Pose pose = with<phys::Body>::get(context, *well).pose();
        if (shell and with<scene::Node>::exists(context, *shell))
            with<scene::Node>::modify(context, *shell)->pose = pose;
        if (atmosphere and with<scene::Node>::exists(context, *atmosphere))
            with<scene::Node>::modify(context, *atmosphere)->pose = pose;
    }

    auto Planet::spin(const phys::Body::Quantum& body) const -> float {
        const dquat relative = glm::normalize(glm::conjugate(dquat{orientationOf(passport)}) * body.orientation);
        const double angle = 2.0 * std::atan2(relative.y, relative.w);
        return float(angle < 0.0 ? angle + 2.0 * std::numbers::pi : angle);
    }

    void Planet::spin(phys::Body::Quantum& body, float value) const {
        body.orientation = glm::normalize(dquat{orientationOf(passport)} * glm::angleAxis(double(value), dvec3{0.0, 1.0, 0.0}));
    }

    auto Planet::reliefScale() const -> float {
        return runtime.reliefAmplitude / float(reliefPeak);
    }

    auto Planet::surfaceRadius(std::int16_t quantum) const -> double {
        return double(passport.radius) + double(quantum) * double(reliefScale());
    }

    auto Planet::encodeRelief(float deltaMeters) const -> std::int16_t {
        const float scale = reliefScale();
        if (scale <= 0.0f)
            return 0;
        return static_cast<std::int16_t>(std::clamp(std::lround(deltaMeters / scale), -long(reliefPeak), long(reliefPeak)));
    }

    auto Planet::height(vec3 dir) const -> double {
        const float len = glm::length(dir);
        if (len < 1.0e-6f)
            return double(passport.radius);
        dir /= len;
        const auto tri = heights.pack.triangle(geo::IcosaPack::locate(dir));
        return double(tri.bary.x) * surfaceRadius(heights.at(tri.a)) + double(tri.bary.y) * surfaceRadius(heights.at(tri.b)) + double(tri.bary.z) * surfaceRadius(heights.at(tri.c));
    }

    auto Planet::altitudeAt(const phys::Body::Quantum& body, dvec3 worldPos) const -> double {
        const dvec3 local = toLocal(body, worldPos);
        const double radial = glm::length(local);
        if (radial < 1.0e-12)
            return -height(vec3{0.0f, 1.0f, 0.0f});
        return radial - height(vec3{glm::normalize(local)});
    }

    auto Planet::gravityAt(const phys::Body::Quantum& body, dvec3 worldPos) const -> dvec3 {
        const dvec3 offset = worldPos - body.position;
        const double distance = glm::length(offset);
        if (distance < 1.0e-12)
            return dvec3{0.0, 0.0, 0.0};
        const double radius = double(passport.radius);
        const double surface = double(runtime.surfaceAcceleration);
        const double accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance * distance * distance);
        return offset * accelScale;
    }

    auto Planet::airDensity(const phys::Body::Quantum& body, dvec3 worldPos) const -> float {
        return phys::Settings::Air::density(float(glm::length(toLocal(body, worldPos))) - passport.radius, runtime.atmosphere.seaDensity, runtime.atmosphere.kerman);
    }

    auto Planet::windAt(const phys::Body::Quantum& body, dvec3 worldPos) const -> dvec3 {
        return glm::cross(spinOmega, worldPos - body.position);
    }

    auto Planet::probe(const phys::Body::Quantum& body, vec3 dir) const -> Probe {
        const float len = glm::length(dir);
        const dquat rotation = body.orientation;
        if (len < 1.0e-6f)
            return Probe{.height = passport.radius, .position = body.position, .normal = rotation * dvec3{0.0, 1.0, 0.0}, .mix = passport.bulk, .slope = 0.0f};
        dir /= len;
        const double radial = height(dir);
        const dvec3 localNormal = heightNormal(*this, dir);
        return Probe{
            .height = radial,
            .position = body.position + rotation * (dvec3{dir} * double(radial)),
            .normal = glm::normalize(rotation * dvec3{localNormal}),
            .mix = passport.bulk,
            .slope = float(1.0 - glm::clamp(glm::dot(localNormal, dvec3{dir}), 0.0, 1.0)),
        };
    }

    auto Planet::surfaceAt(const phys::Body::Quantum& body, dvec3 worldPos) const -> Probe {
        const dvec3 local = toLocal(body, worldPos);
        const double radial = glm::length(local);
        const dquat rotation = body.orientation;
        if (radial < 1.0e-12)
            return probe(body, vec3{0.0f, 1.0f, 0.0f});
        const dvec3 dir = local / radial;
        const vec3 dirF{dir};
        const double h = height(dirF);
        const dvec3 localNormal = heightNormal(*this, dirF);
        return Probe{
            .height = h,
            .position = body.position + rotation * (dir * double(h)),
            .normal = glm::normalize(rotation * dvec3{localNormal}),
            .mix = passport.bulk,
            .slope = float(1.0 - glm::clamp(glm::dot(localNormal, dir), 0.0, 1.0)),
        };
    }

}
