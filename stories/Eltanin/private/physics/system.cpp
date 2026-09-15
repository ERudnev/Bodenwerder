#include "physics/system.h"
#include "physics/collisions.h"
#include "physics/construct.h"
#include "physics/resting.h"
#include "geo/celestial/planet.h"
#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/resting.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eltanin::phys {

    namespace {

        constexpr float minLook = 1.0e-8f;

        auto lookAlong(vec3 forward, quat fallback) -> quat {
            const float length = glm::length(forward);
            if (length < minLook)
                return fallback;
            const vec3 dir = forward / length;
            vec3 up{0.0f, 1.0f, 0.0f};
            if (glm::abs(glm::dot(dir, up)) > 0.99f)
                up = vec3{1.0f, 0.0f, 0.0f};
            return glm::quatLookAt(dir, up);
        }

        auto asleepOnWell(Stewarding context, Body::Id well) -> std::unordered_set<Body::Id> {
            std::unordered_set<Body::Id> asleep{well};
            std::unordered_map<Body::Id, vector<Body::Id>> graph;
            for (auto [_, rest] : context.direct<Resting>().items) {
                graph[rest.first].push_back(rest.second);
                graph[rest.second].push_back(rest.first);
            }
            vector<Body::Id> walk{well};
            for (std::size_t head = 0; head < walk.size(); ++head) {
                const auto found = graph.find(walk[head]);
                if (found == graph.end())
                    continue;
                for (const Body::Id next : found->second) {
                    if (asleep.insert(next).second)
                        walk.push_back(next);
                }
            }
            return asleep;
        }

    }

    auto Settings::Air::density(float altitude, float seaDensity, float kerman) -> float {
        if (seaDensity <= 0.0f or kerman <= 0.0f)
            return 0.0f;
        if (altitude >= kerman * 3.0f)
            return 0.0f;
        if (altitude <= 0.0f)
            return 0.0f;
        const float u = altitude / (kerman * 3.0f);
        return seaDensity * std::pow(1.0f - u * u, kermanPower);
    }

    System::System(scene::Root::Id scene)
        : scene(scene)
        , planet(nullptr)
        , debt(0)
        , thermalDebt(0)
        , collisions{} {
    }

    void System::applyAerodynamics(Stewarding context) {
        if (not planet or not planet->well)
            return;
        auto bodies = context.direct<Body>();
        auto* wellBody = bodies.items.find(*planet->well);
        if (not wellBody)
            return;
        const double dt = Settings::fixedStep;
        const double dt2 = dt * dt;
        const float spinDt = float(dt);
        const float spinTau = Settings::Air::spinHalfLife / 0.693147f;
        auto dragScale = [dt, dt2](float density) -> double {
            double factor = 1.0 - (double(density) / double(Settings::Air::isaDensity)) * (dt / double(Settings::Air::dragTau));
            if (factor < 0.0)
                factor = 0.0;
            if (factor >= 1.0)
                return 0.0;
            return (factor - 1.0) / dt2;
        };
        auto spinGain = [spinDt, spinTau](float density) -> float {
            float spinFactor = 1.0f - (density / Settings::Air::isaDensity) * (spinDt / spinTau);
            if (spinFactor < 0.0f)
                spinFactor = 0.0f;
            if (spinFactor >= 1.0f)
                return 0.0f;
            return (spinFactor - 1.0f) / spinDt;
        };
        for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
            for (Particle& particle : crystal.particles) {
                if (particle.mass <= 0.0f)
                    continue;
                const double scale = dragScale(planet->airDensity(*wellBody, particle.position));
                if (scale == 0.0)
                    continue;
                particle.force += double(particle.mass) * scale * (particle.position - particle.prev - planet->windAt(*wellBody, particle.position) * dt);
            }
        }
        for (auto [_, ray] : context.direct<rigid::Ray>().items) {
            if (ray.core.mass <= 0.0f)
                continue;
            const double scale = dragScale(planet->airDensity(*wellBody, ray.core.position));
            if (scale == 0.0)
                continue;
            ray.core.force += double(ray.core.mass) * scale * (ray.core.position - ray.core.prev - planet->windAt(*wellBody, ray.core.position) * dt);
        }
        for (auto [id, solid] : context.direct<rigid::Solid>().items) {
            auto* body = bodies.items.find(id);
            if (not body or body->totalMass <= 0.0f)
                continue;
            const float density = planet->airDensity(*wellBody, body->position);
            const double linear = dragScale(density);
            if (linear != 0.0)
                solid.center.force += double(body->totalMass) * linear * (body->position - solid.center.prev - planet->windAt(*wellBody, body->position) * dt);
            const float spin = spinGain(density);
            if (spin == 0.0f)
                continue;
            const float inertia = 0.4f * body->totalMass * body->radius * body->radius;
            if (inertia <= 1.0e-12f)
                continue;
            const quat qRel = glm::normalize(body->orientation * glm::conjugate(solid.prevOri));
            vec3 omega = (2.0f / spinDt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            solid.forceAngular += inertia * spin * omega;
        }
    }

    void System::accumulateForces(Stewarding context) {
        for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
            for (Particle& particle : crystal.particles)
                particle.force = dvec3{0.0, 0.0, 0.0};
        }
        for (auto [_, solid] : context.direct<rigid::Solid>().items) {
            solid.center.force = dvec3{0.0, 0.0, 0.0};
            solid.forceAngular = vec3{0.0f, 0.0f, 0.0f};
        }
        for (auto [_, ray] : context.direct<rigid::Ray>().items)
            ray.core.force = dvec3{0.0, 0.0, 0.0};
        applyAerodynamics(context);
        with<rigid::CelestialGravity>::apply(context);
        applyPlanetGravity(context);
        with<locality::Flash>::apply(context);
    }

    void System::applyPlanetGravity(Stewarding context) {
        if (not planet or not planet->well)
            return;
        const auto well = *planet->well;
        auto bodies = context.direct<Body>();
        auto* wellBody = bodies.items.find(well);
        if (not wellBody)
            return;
        const auto asleep = asleepOnWell(context, well);
        for (auto [id, crystal] : context.direct<rigid::Crystal>().items) {
            if (asleep.contains(id))
                continue;
            for (Particle& particle : crystal.particles) {
                if (particle.mass <= 0.0f)
                    continue;
                particle.force += double(particle.mass) * planet->gravityAt(*wellBody, particle.position);
            }
        }
        for (auto [id, solid] : context.direct<rigid::Solid>().items) {
            if (asleep.contains(id))
                continue;
            auto* body = bodies.items.find(id);
            if (not body or body->totalMass <= 0.0f)
                continue;
            solid.center.force += double(body->totalMass) * planet->gravityAt(*wellBody, body->position);
        }
        for (auto [id, ray] : context.direct<rigid::Ray>().items) {
            if (asleep.contains(id) or ray.core.mass <= 0.0f)
                continue;
            ray.core.force += double(ray.core.mass) * planet->gravityAt(*wellBody, ray.core.position);
        }
    }

    void System::integrate(fqsm::Direct<rigid::Crystal> crystals) {
        const double dt2 = Settings::fixedStep * Settings::fixedStep;
        const double rest2 = double(Settings::restLinear) * double(Settings::restLinear);
        for (auto [_, crystal] : crystals.items) {
            for (Particle& particle : crystal.particles) {
                const dvec3 previous = particle.position;
                const dvec3 accel = particle.mass > 0.0f ? particle.force / double(particle.mass) : dvec3{0.0, 0.0, 0.0};
                dvec3 step = particle.position - particle.prev;
                if (glm::dot(step, step) < rest2)
                    step = dvec3{0.0, 0.0, 0.0};
                particle.position += step + accel * dt2;
                particle.prev = previous;
            }
        }
    }

    void System::integrateSolids(fqsm::Direct<Body> bodies, fqsm::Direct<rigid::Solid> solids) {
        const double dt = Settings::fixedStep;
        const double dt2 = dt * dt;
        const double rest2 = double(Settings::restLinear) * double(Settings::restLinear);
        const float spinDt = float(dt);
        for (auto [id, solid] : solids.items) {
            auto* body = bodies.items.find(id);
            if (not body or body->totalMass <= 0.0f)
                continue;

            const dvec3 previousPos = body->position;
            const dvec3 accel = solid.center.force / double(body->totalMass);
            dvec3 step = body->position - solid.center.prev;
            if (glm::dot(step, step) < rest2)
                step = dvec3{0.0, 0.0, 0.0};
            body->position += step + accel * dt2;
            solid.center.prev = previousPos;
            solid.center.position = body->position;

            // Sphere inertia: I = ⅖ m r²
            const float inertia = 0.4f * body->totalMass * body->radius * body->radius;
            if (inertia <= 1.0e-12f)
                continue;

            const quat qRel = glm::normalize(body->orientation * glm::conjugate(solid.prevOri));
            vec3 omega = (2.0f / spinDt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            omega += (solid.forceAngular / inertia) * spinDt;
            if (glm::length(omega) * spinDt < Settings::restLinear)
                omega = vec3{0.0f, 0.0f, 0.0f};

            const quat previousOri = body->orientation;
            const float omegaLen = glm::length(omega);
            if (omegaLen > 1.0e-12f) {
                const quat stepOri = glm::angleAxis(omegaLen * spinDt, omega / omegaLen);
                body->orientation = glm::normalize(stepOri * body->orientation);
            }
            solid.prevOri = previousOri;
        }
    }

    void System::integrateRays(fqsm::Direct<Body> bodies, fqsm::Direct<rigid::Ray> rays) {
        const double dt2 = Settings::fixedStep * Settings::fixedStep;
        const double rest2 = double(Settings::restLinear) * double(Settings::restLinear);
        for (auto [id, ray] : rays.items) {
            auto* body = bodies.items.find(id);
            if (not body or ray.core.mass <= 0.0f)
                continue;
            const dvec3 previous = ray.core.position;
            const dvec3 accel = ray.core.force / double(ray.core.mass);
            dvec3 step = ray.core.position - ray.core.prev;
            if (glm::dot(step, step) < rest2)
                step = dvec3{0.0, 0.0, 0.0};
            ray.core.position += step + accel * dt2;
            ray.core.prev = previous;
            body->position = ray.core.position;
            body->totalMass = ray.core.mass;
            body->orientation = lookAlong(vec3{(ray.core.position - ray.core.prev) / Settings::fixedStep}, body->orientation);
        }
    }

    void System::restoreBases(Stewarding context) {
        with<rigid::Crystal>::restore(context);
    }

    void System::applyConnectivity(Stewarding context) {
        if (planet and planet->well) {
            collisions.well.id = planet->well;
            collisions.well.planet = planet;
        } else {
            collisions.well.id.reset();
            collisions.well.planet = nullptr;
        }
        collisions.build(context);
        if (planet)
            collisions.collidePlanet(context, *planet);
        collisions.solve(context);
        collisions.traceRays(context);
    }

    void System::applyConstructIntegrity(Stewarding context) {
        auto crystals = context.direct<rigid::Crystal>();
        auto bodies = context.direct<Body>();
        for (auto [_, construct] : context.direct<locality::Construct>().items) {
            auto* crystal = crystals.items.find(construct.body);
            auto* body = bodies.items.find(construct.body);
            if (not crystal or not body)
                continue;
            reconcile(construct.construction, *crystal, *body);
        }
    }

    void System::applyConstraintWishes(Stewarding context) {
        with<rigid::Crystal>::applyRestored(context);
    }

    void System::tick(Stewarding context) {
        accumulateForces(context);
        integrate(context.direct<rigid::Crystal>());
        integrateSolids(context.direct<Body>(), context.direct<rigid::Solid>());
        integrateRays(context.direct<Body>(), context.direct<rigid::Ray>());
        restoreBases(context);
        applyConnectivity(context);
        applyConstructIntegrity(context);
        restoreBases(context);
        applyConstraintWishes(context);
        collision::dissipateResting(collisions, context);
    }

    void System::radiate(Stewarding context) {
        if (thermalDebt < Settings::thermalStep)
            return;
        with<::eltanin::locality::geo::Rock>::radiate(context, thermalDebt);
        with<::eltanin::locality::geo::Boulder>::radiate(context, thermalDebt);
        with<::eltanin::locality::Construct>::radiate(context, thermalDebt);
        with<::eltanin::locality::Scrap>::radiate(context, thermalDebt);
        thermalDebt = 0;
    }

    void System::step(establish::Realm& world, seconds dt) {
        debt += dt;
        thermalDebt += dt;
        if (debt < Settings::fixedStep and thermalDebt < Settings::thermalStep)
            return;
        Stewarding session = world;
        if (not with<scene::Root>::exists(session, scene))
            return;
        bool ticked = false;
        const seconds maxDebt = Settings::fixedStep * 10;
        if (debt > maxDebt)
            debt = maxDebt;
        while (debt >= Settings::fixedStep) {
            tick(session);
            debt -= Settings::fixedStep;
            ticked = true;
        }
        radiate(session);
        if (ticked)
            with<locality::Thing>::followBodies(session);
    }

}
