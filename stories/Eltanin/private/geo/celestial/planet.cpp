#include "geo/celestial/planet.h"
#include "geo/celestial/horizon.h"
#include "geo/details/generator.h"
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
#include <vector>

namespace eltanin::locality::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto heightNormal(const Planet& planet, vec3 dir) -> vec3 {
            const float len = glm::length(dir);
            if (len < 1.0e-6f)
                return vec3{0.0f, 1.0f, 0.0f};
            dir /= len;
            vec3 tangentU = glm::cross(vec3{0.0f, 1.0f, 0.0f}, dir);
            if (glm::dot(tangentU, tangentU) < 1.0e-8f)
                tangentU = glm::cross(vec3{1.0f, 0.0f, 0.0f}, dir);
            tangentU = glm::normalize(tangentU);
            const vec3 tangentV = glm::cross(dir, tangentU);
            const float eps = 1.0f / float(std::max(planet.heights.pack.edgeSegments(), integer{1}));
            auto surface = [&](vec3 sample) -> vec3 {
                sample = glm::normalize(sample);
                return sample * planet.height(sample);
            };
            vec3 normal = glm::cross(surface(dir + tangentU * eps) - surface(dir - tangentU * eps), surface(dir + tangentV * eps) - surface(dir - tangentV * eps));
            const float mag = glm::length(normal);
            if (mag < 1.0e-8f)
                return dir;
            normal /= mag;
            if (glm::dot(normal, dir) < 0.0f)
                return -normal;
            return normal;
        }

        auto heightAtlas(const geo::IcosaMap<std::int16_t>& heights) -> vector<std::int16_t> {
            const auto size = heights.pack.atlasSize();
            vector<std::int16_t> pixels(static_cast<std::size_t>(size.x * size.y), std::int16_t{0});
            const integer count = heights.pack.storedCount();
            for (integer index = 0; index < count; ++index) {
                const auto slot = heights.pack.slotOf(index);
                const auto coord = heights.pack.atlasCoord(slot);
                pixels[static_cast<std::size_t>(coord.y * size.x + coord.x)] = heights.at(slot);
            }
            return pixels;
        }

        auto coverAtlas(const geo::IcosaMap<std::uint16_t>& covers) -> vector<std::uint8_t> {
            const auto size = covers.pack.atlasSize();
            vector<std::uint8_t> pixels(static_cast<std::size_t>(size.x * size.y * 2), std::uint8_t{0});
            const integer count = covers.pack.storedCount();
            for (integer index = 0; index < count; ++index) {
                const auto slot = covers.pack.slotOf(index);
                const auto coord = covers.pack.atlasCoord(slot);
                const std::uint16_t packed = covers.at(slot);
                const std::size_t pixel = static_cast<std::size_t>(coord.y * size.x + coord.x) * 2u;
                pixels[pixel] = static_cast<std::uint8_t>(packed & 255u);
                pixels[pixel + 1] = static_cast<std::uint8_t>(packed >> 8);
            }
            return pixels;
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

        auto patchDescriptors(const geo::IcosaPack& pack, integer cells) -> vector<scene::actor::PatchGrid::Patch> {
            vector<scene::actor::PatchGrid::Patch> patches;
            const integer segments = pack.edgeSegments();
            for (integer diamond = 0; diamond < geo::IcosaPack::diamondCount; ++diamond) {
                for (integer originV = 0; originV < segments; originV += cells) {
                    for (integer originU = 0; originU < segments; originU += cells)
                        patches.push_back(scene::actor::PatchGrid::Patch{.diamond = diamond, .originU = originU, .originV = originV, .step = 1});
                }
            }
            return patches;
        }

        void bindAtmosphereMesh(scene::actor::MeshState::Quantum& mesh, const Passport& passport) {
            mesh.albedo = passport.atmosphere.day;
            mesh.opacity = passport.atmosphere.seaDensity;
            mesh.heat = vec2{passport.radius, passport.atmosphere.outerRadius};
            mesh.scale = vec3{passport.atmosphere.outerRadius * 1.08f};
            mesh.latticeStep = 0.0f;
            mesh.patternScale = geo::Horizon::locality;
        }

        auto spawnAtmosphere(Writing context, Pose pose, const Passport& passport) -> base::maybe<scene::actor::Mesh::Id> {
            if (passport.atmosphere.seaDensity <= 0.0f or passport.atmosphere.outerRadius <= passport.radius)
                return {};
            const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "atmosphere"));
            if (not material) {
                context.refuse("eltanin::locality::planet::Planet::place: atmosphere material missing");
                return {};
            }
            const auto sphere = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "atmosphereSphere"));
            if (not sphere) {
                context.refuse("eltanin::locality::planet::Planet::place: atmosphereSphere geometry missing");
                return {};
            }
            auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, *sphere, *material);
            if (not meshQuantum) {
                context.refuse("eltanin::locality::planet::Planet::place: atmosphere mesh compose failed");
                return {};
            }
            auto meshState = with<scene::actor::MeshState>::defaults(passport.atmosphere.day, passport.atmosphere.seaDensity, vec3{1.0f});
            bindAtmosphereMesh(meshState, passport);
            return with<scene::Interface>::createMeshActor(context, with<Thing>::get_global(context).scene, pose, std::move(*meshQuantum), meshState);
        }

        auto toLocal(const Planet& planet, dvec3 worldPos) -> dvec3 {
            return glm::inverse(glm::dquat{planet.pose.rotation}) * (worldPos - dvec3{planet.pose.position});
        }

        void applySpin(Planet& planet) {
            planet.pose.rotation = planet.passport.orientation * glm::angleAxis(planet.spin, vec3{0.0f, 1.0f, 0.0f});
        }

        auto wellQuantum(const Planet& planet) -> phys::Body::Quantum {
            const float radius = planet.passport.radius;
            const float volume = (4.0f / 3.0f) * std::numbers::pi_v<float> * radius * radius * radius;
            return phys::Body::Quantum{
                .position = dvec3{planet.pose.position},
                .orientation = planet.pose.rotation,
                .totalMass = volume * 3000.0f,
                .radius = radius + planet.passport.geology.amplitude,
                .compound = phys::Body::Id::please_never_use_this_except_patch_rejection_mechanism(),
            };
        }

        void bindWell(phys::Body::Quantum& body, const Planet& planet) {
            const auto next = wellQuantum(planet);
            body.position = next.position;
            body.orientation = next.orientation;
            body.totalMass = next.totalMass;
            body.radius = next.radius;
        }

    }

    auto Planet::recommendedDetail(float radius, float edge) -> Detail {
        constexpr integer maxShellTriangles = 20 * 512 * 512; // full-resolution shell budget; N = edgeBase * 2^t
        const integer cap = static_cast<integer>(std::sqrt(static_cast<double>(maxShellTriangles) / 20.0));
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
        , pose{.position = Pos{0.0f, 0.0f, 0.0f}, .rotation = passport.orientation}
        , spin{0.0f}
        , well{}
        , heights{geo::IcosaPack{.edgeBase = detail.edgeBase, .tessellation = detail.tessellation}, std::int16_t{0}}
        , covers{heights.pack, std::uint16_t{0}}
        , shell{}
        , atmosphere{} {
        geo::generate(*this);
        applySpin(*this);
    }

    void Planet::place(Writing context, system::Device::Id device, Pose pose) {
        this->pose.position = pose.position;
        applySpin(*this);
        const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "planet"));
        if (not material) {
            context.refuse("eltanin::locality::planet::Planet::place: planet material missing");
            return;
        }
        const auto grid = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "patchGrid"));
        if (not grid) {
            context.refuse("eltanin::locality::planet::Planet::place: patchGrid geometry missing");
            return;
        }
        const auto facies = with<resource::Assets>::find<resource::texpack::Pack>(context, resource::Unit::Name::from("Eltanin", "facies"));
        if (not facies) {
            context.refuse("eltanin::locality::planet::Planet::place: facies texpack missing");
            return;
        }
        const auto manager = with<resource::Manager>::singleton(context);
        const auto atlas = heights.pack.atlasSize();
        const auto heightPixels = heightAtlas(heights);
        const auto coverPixels = coverAtlas(covers);
        const auto heightId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-height")});
        with<resource::texture::Asset>::extend(context, heightId, resource::texture::Asset::Quantum{});
        const auto heightBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(heightPixels.data()), heightPixels.size() * sizeof(std::int16_t));
        if (not with<resource::texture::Asset>::install(context, heightId, device, resource::texture::Asset::Format::r16Snorm, atlas, heightBytes)) {
            context.refuse("eltanin::locality::planet::Planet::place: height atlas install failed");
            return;
        }
        const auto coverId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-cover")});
        with<resource::texture::Asset>::extend(context, coverId, resource::texture::Asset::Quantum{});
        const auto coverBytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(coverPixels.data()), coverPixels.size());
        if (not with<resource::texture::Asset>::install(context, coverId, device, resource::texture::Asset::Format::rg8, atlas, coverBytes)) {
            context.refuse("eltanin::locality::planet::Planet::place: cover atlas install failed");
            return;
        }
        constexpr integer cells = 32;
        const auto patches = patchDescriptors(heights.pack, cells);
        auto gridQuantum = with<scene::actor::PatchGrid>::compose(context, *grid, *material, *facies, heightId, coverId, icosaShell(), patches, passport.radius, passport.geology.amplitude, heights.pack.edgeVertices(), cells);
        if (not gridQuantum) {
            context.refuse("eltanin::locality::planet::Planet::place: patch grid compose failed");
            return;
        }
        shell = with<scene::Interface>::createPatchGridActor(context, with<Thing>::get_global(context).scene, this->pose, std::move(*gridQuantum));
        with<scene::Root>::modify(context, with<Thing>::get_global(context).scene)->atmosphereDensity = passport.atmosphere.seaDensity;
        with<scene::Root>::modify(context, with<Thing>::get_global(context).scene)->atmosphereKerman = passport.atmosphere.kerman;
        atmosphere = spawnAtmosphere(context, this->pose, passport);
        if (not well)
            well = phys::createBody(context, wellQuantum(*this), {});
        else
            bindWell(*with<phys::Body>::modify(context, *well), *this);
        sync(context);
    }

    void Planet::update(Writing, Pos) {
    }

    void Planet::sync(Writing context) {
        applySpin(*this);
        if (shell and with<scene::Node>::exists(context, *shell))
            with<scene::Node>::modify(context, *shell)->pose = pose;
        if (atmosphere and with<scene::Node>::exists(context, *atmosphere))
            with<scene::Node>::modify(context, *atmosphere)->pose = pose;
        if (well and with<phys::Body>::exists(context, *well))
            bindWell(*with<phys::Body>::modify(context, *well), *this);
    }

    auto Planet::reliefScale() const -> float {
        return passport.geology.amplitude / float(reliefPeak);
    }

    auto Planet::surfaceRadius(std::int16_t quantum) const -> float {
        return passport.radius + float(quantum) * reliefScale();
    }

    auto Planet::encodeRelief(float deltaMeters) const -> std::int16_t {
        const float scale = reliefScale();
        if (scale <= 0.0f)
            return 0;
        return static_cast<std::int16_t>(std::clamp(std::lround(deltaMeters / scale), -long(reliefPeak), long(reliefPeak)));
    }

    auto Planet::height(vec3 dir) const -> float {
        const float len = glm::length(dir);
        if (len < 1.0e-6f)
            return passport.radius;
        dir /= len;
        const auto tri = heights.pack.triangle(geo::IcosaPack::locate(dir));
        return tri.bary.x * surfaceRadius(heights.at(tri.a)) + tri.bary.y * surfaceRadius(heights.at(tri.b)) + tri.bary.z * surfaceRadius(heights.at(tri.c));
    }

    auto Planet::altitudeAt(Pos worldPos) const -> float {
        const dvec3 local = toLocal(*this, dvec3{worldPos});
        const double radial = glm::length(local);
        if (radial < 1.0e-12)
            return -height(vec3{0.0f, 1.0f, 0.0f});
        return float(radial - double(height(vec3{glm::normalize(local)})));
    }

    auto Planet::gravityAt(dvec3 worldPos) const -> dvec3 {
        const dvec3 offset = worldPos - dvec3{pose.position};
        const double distance = glm::length(offset);
        if (distance < 1.0e-12)
            return dvec3{0.0, 0.0, 0.0};
        const double radius = double(passport.radius);
        const double surface = double(passport.surfaceAcceleration);
        const double accelScale = distance < radius ? -surface / radius : -surface * radius * radius / (distance * distance * distance);
        return offset * accelScale;
    }

    auto Planet::airDensity(Pos worldPos) const -> float {
        return phys::Settings::Air::density(float(glm::length(toLocal(*this, dvec3{worldPos}))) - passport.radius, passport.atmosphere.seaDensity, passport.atmosphere.kerman);
    }

    auto Planet::windAt(dvec3) const -> dvec3 {
        return dvec3{0.0, 0.0, 0.0};
    }

    auto Planet::probe(vec3 dir) const -> Probe {
        const float len = glm::length(dir);
        const glm::dquat rotation{pose.rotation};
        if (len < 1.0e-6f)
            return Probe{.height = passport.radius, .position = dvec3{pose.position}, .normal = rotation * dvec3{0.0, 1.0, 0.0}, .mix = passport.geology.mix, .slope = 0.0f};
        dir /= len;
        const float radial = height(dir);
        const vec3 localNormal = heightNormal(*this, dir);
        return Probe{
            .height = radial,
            .position = dvec3{pose.position} + rotation * (dvec3{dir} * double(radial)),
            .normal = glm::normalize(rotation * dvec3{localNormal}),
            .mix = passport.geology.mix,
            .slope = 1.0f - glm::clamp(glm::dot(localNormal, dir), 0.0f, 1.0f),
        };
    }

}
