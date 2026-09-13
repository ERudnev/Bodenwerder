#include "geo/celestial/planet.h"
#include "geo/celestial/horizon.h"
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
#include <numbers>

namespace eltanin::locality::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        void emitFace(resource::builders::geometry::CpuPresentation& cpu, vec3 first, vec3 second, vec3 third) {
            vec3 normal = glm::cross(second - first, third - first);
            if (glm::dot(normal, first + second + third) < 0.0f) {
                const vec3 swap = second;
                second = third;
                third = swap;
                normal = -normal;
            }
            normal = glm::normalize(normal);
            cpu.positions.push_back(first);
            cpu.positions.push_back(second);
            cpu.positions.push_back(third);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
        }

        auto atHeight(const geo::IcosaPack& pack, const vector<float>& heights, geo::IcosaPack::Slot slot) -> float {
            return heights[static_cast<std::size_t>(pack.index(slot))];
        }

        auto vertexAt(const geo::IcosaPack& pack, const vector<float>& heights, geo::IcosaPack::Slot slot) -> vec3 {
            return pack.direction(slot) * atHeight(pack, heights, slot);
        }

        auto shellMesh(const geo::IcosaPack& pack, const vector<float>& heights) -> resource::builders::geometry::CpuPresentation {
            const integer span = pack.edgeSegments();
            resource::builders::geometry::CpuPresentation cpu{
                .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal"}),
                .positions = {},
                .normals = {},
                .uv0 = {},
                .color0 = {},
                .indices = {},
                .mix0 = {},
                .cohesion = {},
            };
            cpu.positions.reserve(static_cast<std::size_t>(geo::IcosaPack::faceCount * 3));
            cpu.normals.reserve(cpu.positions.capacity());
            for (integer diamond = 0; diamond < geo::IcosaPack::diamondCount; ++diamond) {
                const vec3 top = vertexAt(pack, heights, geo::IcosaPack::Slot{.diamond = diamond, .iu = 0, .iv = 0});
                const vec3 right = vertexAt(pack, heights, geo::IcosaPack::Slot{.diamond = diamond, .iu = span, .iv = 0});
                const vec3 left = vertexAt(pack, heights, geo::IcosaPack::Slot{.diamond = diamond, .iu = 0, .iv = span});
                const vec3 bottom = vertexAt(pack, heights, geo::IcosaPack::Slot{.diamond = diamond, .iu = span, .iv = span});
                emitFace(cpu, top, right, left);
                emitFace(cpu, right, bottom, left);
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

    Planet::Planet(Passport passport)
        : passport{passport}
        , pose{.position = Pos{0.0f, 0.0f, 0.0f}, .rotation = passport.orientation}
        , spin{0.0f}
        , well{}
        , pack{.edgeBase = 1, .tessellation = 0}
        , heights{}
        , shell{}
        , atmosphere{} {
        heights.assign(static_cast<std::size_t>(pack.storedCount()), this->passport.radius);
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
        if (not with<resource::geometry::Asset>::install(context, geometryId, device, shellMesh(pack, heights))) {
            context.refuse("eltanin::locality::planet::Planet::place: geometry install failed");
            return;
        }
        auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, geometryId, *material);
        if (not meshQuantum) {
            context.refuse("eltanin::locality::planet::Planet::place: mesh compose failed");
            return;
        }
        auto meshState = with<scene::actor::MeshState>::defaults(RGB{0.29f, 0.31f, 0.20f}, 1.0f);
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
        const auto sample = pack.locate(dir);
        const integer last = pack.edgeSegments();
        const float fu = sample.u * static_cast<float>(last);
        const float fv = sample.v * static_cast<float>(last);
        const integer iu0 = std::clamp(static_cast<integer>(std::floor(fu)), integer{0}, last);
        const integer iv0 = std::clamp(static_cast<integer>(std::floor(fv)), integer{0}, last);
        const integer iu1 = std::min(iu0 + 1, last);
        const integer iv1 = std::min(iv0 + 1, last);
        const float tu = fu - static_cast<float>(iu0);
        const float tv = fv - static_cast<float>(iv0);
        const float top = glm::mix(atHeight(pack, heights, geo::IcosaPack::Slot{.diamond = sample.diamond, .iu = iu0, .iv = iv0}), atHeight(pack, heights, geo::IcosaPack::Slot{.diamond = sample.diamond, .iu = iu1, .iv = iv0}), tu);
        const float bottom = glm::mix(atHeight(pack, heights, geo::IcosaPack::Slot{.diamond = sample.diamond, .iu = iu0, .iv = iv1}), atHeight(pack, heights, geo::IcosaPack::Slot{.diamond = sample.diamond, .iu = iu1, .iv = iv1}), tu);
        return glm::mix(top, bottom, tv);
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
        if (len < 1.0e-6f)
            return Probe{.height = passport.radius, .position = dvec3{pose.position}, .normal = glm::dquat{pose.rotation} * dvec3{0.0, 1.0, 0.0}, .mix = passport.geology.mix, .slope = 0.0f};
        dir /= len;
        const float radial = height(dir);
        const glm::dquat rotation{pose.rotation};
        return Probe{.height = radial, .position = dvec3{pose.position} + rotation * (dvec3{dir} * double(radial)), .normal = glm::normalize(rotation * dvec3{dir}), .mix = passport.geology.mix, .slope = 0.0f};
    }

}
