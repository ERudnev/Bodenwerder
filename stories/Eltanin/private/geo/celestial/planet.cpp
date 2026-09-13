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
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/geometry.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>

namespace eltanin::locality::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto packRgb(RGB color) -> float {
            const std::uint32_t bits = std::uint32_t(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f + 0.5f) | (std::uint32_t(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f + 0.5f) << 8) | (std::uint32_t(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f + 0.5f) << 16);
            float packed;
            std::memcpy(&packed, &bits, sizeof(packed));
            return packed;
        }

        auto heightNormal(const geo::IcosaMap<float>& heights, vec3 dir) -> vec3 {
            const float len = glm::length(dir);
            if (len < 1.0e-6f)
                return vec3{0.0f, 1.0f, 0.0f};
            dir /= len;
            vec3 tangentU = glm::cross(vec3{0.0f, 1.0f, 0.0f}, dir);
            if (glm::dot(tangentU, tangentU) < 1.0e-8f)
                tangentU = glm::cross(vec3{1.0f, 0.0f, 0.0f}, dir);
            tangentU = glm::normalize(tangentU);
            const vec3 tangentV = glm::cross(dir, tangentU);
            const float eps = 1.0f / float(std::max(heights.pack.edgeSegments(), integer{1}));
            auto surface = [&](vec3 sample) -> vec3 {
                sample = glm::normalize(sample);
                return sample * heights.at(sample);
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

        void emitFace(resource::builders::geometry::CpuPresentation& cpu, vec3 first, vec3 second, vec3 third, vec3 normalA, vec3 normalB, vec3 normalC, RGB colorA, RGB colorB, RGB colorC) {
            if (glm::dot(glm::cross(second - first, third - first), first + second + third) < 0.0f) {
                const vec3 swap = second;
                second = third;
                third = swap;
                const vec3 swapNormal = normalB;
                normalB = normalC;
                normalC = swapNormal;
                const RGB swapColor = colorB;
                colorB = colorC;
                colorC = swapColor;
            }
            const vec4 packed{packRgb(colorA), packRgb(colorB), packRgb(colorC), 1.0f};
            cpu.positions.push_back(first);
            cpu.positions.push_back(second);
            cpu.positions.push_back(third);
            cpu.normals.push_back(normalA);
            cpu.normals.push_back(normalB);
            cpu.normals.push_back(normalC);
            cpu.color0.push_back(packed);
            cpu.color0.push_back(packed);
            cpu.color0.push_back(packed);
        }

        auto vertexAt(const geo::IcosaMap<float>& heights, geo::IcosaPack::Slot slot) -> vec3 {
            return heights.pack.direction(slot) * heights.at(slot);
        }

        auto shellMesh(const geo::IcosaMap<float>& heights, const geo::IcosaMap<RGB>& colors) -> resource::builders::geometry::CpuPresentation {
            const integer last = heights.pack.edgeSegments();
            resource::builders::geometry::CpuPresentation cpu{
                .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "color0"}),
                .positions = {},
                .normals = {},
                .uv0 = {},
                .color0 = {},
                .indices = {},
                .mix0 = {},
                .cohesion = {},
            };
            cpu.positions.reserve(static_cast<std::size_t>(geo::IcosaPack::diamondCount * last * last * 6));
            cpu.normals.reserve(cpu.positions.capacity());
            cpu.color0.reserve(cpu.positions.capacity());
            for (integer diamond = 0; diamond < geo::IcosaPack::diamondCount; ++diamond) {
                for (integer iv = 0; iv < last; ++iv) {
                    for (integer iu = 0; iu < last; ++iu) {
                        const auto up = heights.pack.upper(diamond, iu, iv);
                        const auto down = heights.pack.lower(diamond, iu, iv);
                        emitFace(cpu, vertexAt(heights, up[0]), vertexAt(heights, up[1]), vertexAt(heights, up[2]), heightNormal(heights, heights.pack.direction(up[0])), heightNormal(heights, heights.pack.direction(up[1])), heightNormal(heights, heights.pack.direction(up[2])), colors.at(up[0]), colors.at(up[1]), colors.at(up[2]));
                        emitFace(cpu, vertexAt(heights, down[0]), vertexAt(heights, down[1]), vertexAt(heights, down[2]), heightNormal(heights, heights.pack.direction(down[0])), heightNormal(heights, heights.pack.direction(down[1])), heightNormal(heights, heights.pack.direction(down[2])), colors.at(down[0]), colors.at(down[1]), colors.at(down[2]));
                    }
                }
            }
            return cpu;
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
                .radius = radius + planet.passport.geology.maxRelief,
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
        , heights{geo::IcosaPack{.edgeBase = detail.edgeBase, .tessellation = detail.tessellation}, passport.radius}
        , colors{heights.pack, RGB{1.0f, 1.0f, 1.0f}}
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
        const auto manager = with<resource::Manager>::singleton(context);
        const auto geometryId = with<resource::Unit_group>::addElement(context, manager, resource::Unit::Quantum{.name = resource::Unit::Name::from("Eltanin", "planet-shell")});
        with<resource::geometry::Asset>::extend(context, geometryId, resource::geometry::Asset::Quantum{});
        if (not with<resource::geometry::Asset>::install(context, geometryId, device, shellMesh(heights, colors))) {
            context.refuse("eltanin::locality::planet::Planet::place: geometry install failed");
            return;
        }
        auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, geometryId, *material);
        if (not meshQuantum) {
            context.refuse("eltanin::locality::planet::Planet::place: mesh compose failed");
            return;
        }
        auto meshState = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        meshState.latticeStep = 0.0f;
        shell = with<scene::Interface>::createMeshActor(context, with<Thing>::get_global(context).scene, this->pose, std::move(*meshQuantum), meshState);
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

    auto Planet::height(vec3 dir) const -> float {
        const float len = glm::length(dir);
        if (len < 1.0e-6f)
            return passport.radius;
        return heights.at(dir);
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
        const vec3 localNormal = heightNormal(heights, dir);
        return Probe{
            .height = radial,
            .position = dvec3{pose.position} + rotation * (dvec3{dir} * double(radial)),
            .normal = glm::normalize(rotation * dvec3{localNormal}),
            .mix = passport.geology.mix,
            .slope = 1.0f - glm::clamp(glm::dot(localNormal, dir), 0.0f, 1.0f),
        };
    }

}
