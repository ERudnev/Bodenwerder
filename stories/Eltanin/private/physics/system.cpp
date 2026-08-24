#include "physics/system.h"

#include <eltanin/geo/rock.q1.h>
#include <eltanin/geo/boulder.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin::phys {

    System::System()
        : state{.timeScale = 1.0f}
        , debtUs(0)
        , thermalDebtUs(0) {
    }

    void System::applyAerodynamics(Stewarding context) {
        float density = 0.0f;
        for (auto [_, root] : context.direct<scene::Root>().items) {
            density = root.atmosphereDensity;
            break;
        }
        if (density <= 0.0f)
            return;
        float factor = 1.0f - (density / Settings::isaAirDensity) * (Particle::dt / Settings::airDragTau);
        if (factor >= 1.0f)
            return;
        if (factor < 0.0f)
            factor = 0.0f;
        const float dt2 = Particle::dt * Particle::dt;
        const float dragScale = (factor - 1.0f) / dt2;
        for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
            for (Particle& particle : crystal.particles) {
                if (particle.mass <= 0.0f)
                    continue;
                particle.force += particle.mass * dragScale * (particle.position - particle.prev);
            }
        }
        for (auto [_, ball] : context.direct<rigid::Ball>().items) {
            rigid::Ball::Data& body = ball.body;
            if (body.mass <= 0.0f)
                continue;
            body.forceLinear += body.mass * dragScale * (body.position - body.prevPos);
        }
    }

    void System::accumulateForces(Stewarding context) {
        for (auto [_, crystal] : context.direct<rigid::Crystal>().items) {
            for (Particle& particle : crystal.particles)
                particle.force = vec3{0.0f, 0.0f, 0.0f};
        }
        for (auto [_, ball] : context.direct<rigid::Ball>().items) {
            ball.body.forceLinear = vec3{0.0f, 0.0f, 0.0f};
            ball.body.forceAngular = vec3{0.0f, 0.0f, 0.0f};
        }
        applyAerodynamics(context);
        with<rigid::CelestialGravity>::apply(context);
    }

    void System::integrate(fqsm::Direct<rigid::Crystal> crystals) {
        const float dt2 = Particle::dt * Particle::dt;
        const float rest2 = Settings::restLinear * Settings::restLinear;
        for (auto [_, crystal] : crystals.items) {
            for (Particle& particle : crystal.particles) {
                const vec3 previous = particle.position;
                const vec3 accel = particle.mass > 0.0f ? particle.force / particle.mass : vec3{0.0f, 0.0f, 0.0f};
                vec3 step = (particle.position - particle.prev) * Settings::dissipation;
                if (glm::dot(step, step) < rest2)
                    step = vec3{0.0f, 0.0f, 0.0f};
                particle.position += step + accel * dt2;
                particle.prev = previous;
            }
        }
    }

    void System::integrateBalls(fqsm::Direct<rigid::Ball> balls) {
        const float dt = Particle::dt;
        const float dt2 = dt * dt;
        const float rest2 = Settings::restLinear * Settings::restLinear;
        for (auto [_, ball] : balls.items) {
            rigid::Ball::Data& body = ball.body;
            if (body.mass <= 0.0f)
                continue;

            const vec3 previousPos = body.position;
            const vec3 accel = body.forceLinear / body.mass;
            vec3 step = (body.position - body.prevPos) * Settings::dissipation;
            if (glm::dot(step, step) < rest2)
                step = vec3{0.0f, 0.0f, 0.0f};
            body.position += step + accel * dt2;
            body.prevPos = previousPos;

            // Solid sphere: I = ⅖ m r²
            const float inertia = 0.4f * body.mass * body.radius * body.radius;
            if (inertia <= 1.0e-12f)
                continue;

            const quat qRel = glm::normalize(body.orientation * glm::conjugate(body.prevOri));
            vec3 omega = (2.0f / dt) * vec3{qRel.x, qRel.y, qRel.z};
            if (qRel.w < 0.0f)
                omega = -omega;
            omega *= Settings::dissipation;
            omega += (body.forceAngular / inertia) * dt;
            if (glm::length(omega) * dt < Settings::restLinear)
                omega = vec3{0.0f, 0.0f, 0.0f};

            const quat previousOri = body.orientation;
            const float omegaLen = glm::length(omega);
            if (omegaLen > 1.0e-12f) {
                const quat stepOri = glm::angleAxis(omegaLen * dt, omega / omegaLen);
                body.orientation = glm::normalize(stepOri * body.orientation);
            }
            body.prevOri = previousOri;
        }
    }

    void System::restoreBases(Stewarding context) {
        with<rigid::Crystal>::restore(context);
    }

    void System::applyConnectivity(Stewarding) {}

    void System::applyConstraintWishes(Stewarding context) {
        with<rigid::Crystal>::applyRestored(context);
    }

    void System::tick(Stewarding context) {
        accumulateForces(context);
        integrate(context.direct<rigid::Crystal>());
        integrateBalls(context.direct<rigid::Ball>());
        restoreBases(context);
        applyConnectivity(context);
        applyConstraintWishes(context);
    }

    void System::radiate(Stewarding context) {
        if (thermalDebtUs < Settings::thermalStepUs)
            return;
        with<::eltanin::geo::Rock>::radiate(context, static_cast<float>(thermalDebtUs) * 1e-6f);
        with<::eltanin::geo::Boulder>::radiate(context, static_cast<float>(thermalDebtUs) * 1e-6f);
        thermalDebtUs = 0;
    }

    void System::step(establish::Realm& world, int64 dtUs) {
        const int64 scaled = static_cast<int64>(static_cast<double>(dtUs) * static_cast<double>(state.timeScale));
        debtUs += scaled;
        thermalDebtUs += scaled;
        if (debtUs < Settings::fixedStepUs and thermalDebtUs < Settings::thermalStepUs)
            return;
        Stewarding session = world;
        while (debtUs >= Settings::fixedStepUs) {
            tick(session);
            debtUs -= Settings::fixedStepUs;
        }
        radiate(session);
    }

}
