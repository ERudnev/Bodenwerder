#include "physics/system.h"
#include "physics/collisions.h"

#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

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

    }

    System::System(scene::Root::Id scene)
        : scene(scene)
        , debt(0)
        , thermalDebt(0)
        , collisions{} {
    }

    void System::applyAerodynamics(Stewarding context) {
        const auto& root = with<scene::Root>::get(context, scene);
        const float density = root.atmosphereDensity;
        if (density <= 0.0f)
            return;
        const float densityRatio = density / Settings::Air::isaDensity;
        float factor = 1.0f - densityRatio * (float(Settings::fixedStep) / Settings::Air::dragTau);
        if (factor < 0.0f)
            factor = 0.0f;
        const float spinTau = Settings::Air::spinHalfLife / 0.693147f;
        float spinFactor = 1.0f - densityRatio * (float(Settings::fixedStep) / spinTau);
        if (spinFactor < 0.0f)
            spinFactor = 0.0f;
        if (factor >= 1.0f and spinFactor >= 1.0f)
            return;
        const float dt = float(Settings::fixedStep);
        const float dt2 = dt * dt;
        const float dragScale = (factor - 1.0f) / dt2;
        auto bodies = context.direct<Body>();
        if (factor < 1.0f) {
            for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
                for (Particle& particle : crystal.particles) {
                    if (particle.mass <= 0.0f)
                        continue;
                    particle.force += particle.mass * dragScale * vec3{particle.position - particle.prev};
                }
            }
            for (auto [_, ray] : context.direct<rigid::Ray>().items) {
                if (ray.core.mass <= 0.0f)
                    continue;
                ray.core.force += ray.core.mass * dragScale * vec3{ray.core.position - ray.core.prev};
            }
        }
        for (auto [id, solid] : context.direct<rigid::Solid>().items) {
            auto* body = bodies.items.find(id);
            if (not body or body->totalMass <= 0.0f)
                continue;
            if (factor < 1.0f)
                solid.center.force += body->totalMass * dragScale * vec3{body->position - solid.center.prev};
            if (spinFactor >= 1.0f)
                continue;
            const float inertia = 0.4f * body->totalMass * body->radius * body->radius;
            if (inertia <= 1.0e-12f)
                continue;
            const quat qRel = glm::normalize(body->orientation * glm::conjugate(solid.prevOri));
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            solid.forceAngular += inertia * ((spinFactor - 1.0f) / dt) * omega;
        }
    }

    void System::accumulateForces(Stewarding context) {
        for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
            for (Particle& particle : crystal.particles)
                particle.force = vec3{0.0f, 0.0f, 0.0f};
        }
        for (auto [_, solid] : context.direct<rigid::Solid>().items) {
            solid.center.force = vec3{0.0f, 0.0f, 0.0f};
            solid.forceAngular = vec3{0.0f, 0.0f, 0.0f};
        }
        for (auto [_, ray] : context.direct<rigid::Ray>().items)
            ray.core.force = vec3{0.0f, 0.0f, 0.0f};
        applyAerodynamics(context);
        applyLinearGravity(context);
        with<rigid::CelestialGravity>::apply(context);
        with<locality::Flash>::apply(context);
    }

    void System::applyLinearGravity(Stewarding context) {
        const auto& root = with<scene::Root>::get(context, scene);
        const vec3 gravity = root.gravity;
        if (glm::dot(gravity, gravity) == 0.0f)
            return;
        for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
            for (Particle& particle : crystal.particles) {
                if (particle.mass <= 0.0f)
                    continue;
                particle.force += particle.mass * gravity;
            }
        }
        auto bodies = context.direct<Body>();
        for (auto [id, solid] : context.direct<rigid::Solid>().items) {
            auto* body = bodies.items.find(id);
            if (not body or body->totalMass <= 0.0f)
                continue;
            solid.center.force += body->totalMass * gravity;
        }
        for (auto [_, ray] : context.direct<rigid::Ray>().items) {
            if (ray.core.mass <= 0.0f)
                continue;
            ray.core.force += ray.core.mass * gravity;
        }
    }

    void System::integrate(fqsm::Direct<rigid::Crystal> crystals) {
        const double dt2 = Settings::fixedStep * Settings::fixedStep;
        const double rest2 = double(Settings::restLinear) * double(Settings::restLinear);
        for (auto [_, crystal] : crystals.items) {
            for (Particle& particle : crystal.particles) {
                const dvec3 previous = particle.position;
                const dvec3 accel = particle.mass > 0.0f ? dvec3{particle.force / particle.mass} : dvec3{0.0, 0.0, 0.0};
                dvec3 step = particle.position - particle.prev;
                if (glm::dot(step, step) < rest2)
                    step = dvec3{0.0, 0.0, 0.0};
                particle.position += step + accel * dt2;
                particle.prev = previous;
            }
        }
    }

    void System::integrateSolids(fqsm::Direct<Body> bodies, fqsm::Direct<rigid::Solid> solids) {
        const float dt = float(Settings::fixedStep);
        const double dt2 = double(dt) * double(dt);
        const double rest2 = double(Settings::restLinear) * double(Settings::restLinear);
        for (auto [id, solid] : solids.items) {
            auto* body = bodies.items.find(id);
            if (not body or body->totalMass <= 0.0f)
                continue;

            const dvec3 previousPos = body->position;
            const dvec3 accel = dvec3{solid.center.force / body->totalMass};
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
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            omega += (solid.forceAngular / inertia) * dt;
            if (glm::length(omega) * dt < Settings::restLinear)
                omega = vec3{0.0f, 0.0f, 0.0f};

            const quat previousOri = body->orientation;
            const float omegaLen = glm::length(omega);
            if (omegaLen > 1.0e-12f) {
                const quat stepOri = glm::angleAxis(omegaLen * dt, omega / omegaLen);
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
            const dvec3 accel = dvec3{ray.core.force / ray.core.mass};
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
        collisions.build(context);
        collisions.solve(context);
        collisions.traceRays(context);
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
        restoreBases(context);
        applyConstraintWishes(context);
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
