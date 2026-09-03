#include "physics/construct.h"
#include "physics/settings.h"
#include "physics/verlet.h"
#include "mech/construction.h"

#include <glm/geometric.hpp>

#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using rigid::Crystal;

    namespace {

        void restoreEdge(const Crystal::Quantum& crystal, std::size_t start, std::size_t end, std::size_t origin, double stiff, vector<dvec3>& kickSum, vector<integer>& kickHits, float& peakStrain) {
            if (start >= crystal.particles.size() or end >= crystal.particles.size())
                return;
            const Particle& first = crystal.particles[start];
            const Particle& second = crystal.particles[end];
            if (first.mass <= 0.0f or second.mass <= 0.0f)
                return;
            const dvec3 offset = second.position - first.position;
            const double length = glm::length(offset);
            if (length < 1.0e-12)
                return;
            const double restLength = glm::length(dvec3{crystal.shape[start]} - dvec3{crystal.shape[end]});
            if (restLength < 1.0e-12)
                return;
            const float strain = float(glm::abs(length - restLength) / restLength);
            if (strain > peakStrain)
                peakStrain = strain;
            const dvec3 axis = offset / length;
            const double mass = double(first.mass + second.mass);
            const dvec3 com = (first.position * double(first.mass) + second.position * double(second.mass)) / mass;
            kickSum[start - origin] += (com - axis * (restLength * 0.5) - first.position) * stiff;
            kickSum[end - origin] += (com + axis * (restLength * 0.5) - second.position) * stiff;
            ++kickHits[start - origin];
            ++kickHits[end - origin];
        }

        void restorePrimitive(Crystal::Quantum& crystal, const mech::Construction::Primitive& primitive, std::size_t cursor, double stiff) {
            const std::size_t count = primitive.loop.size();
            if (count < 2 or cursor + count > crystal.particles.size() or cursor + count > crystal.shape.size())
                return;
            vector<dvec3> kickSum(count, dvec3{0.0, 0.0, 0.0});
            vector<integer> kickHits(count, 0);
            float peakStrain = 0.0f;
            if (count == 2)
                restoreEdge(crystal, cursor, cursor + 1, cursor, stiff, kickSum, kickHits, peakStrain);
            else {
                for (std::size_t slot = 0; slot < count; ++slot)
                    restoreEdge(crystal, cursor + slot, cursor + (slot + 1) % count, cursor, stiff, kickSum, kickHits, peakStrain);
            }
            for (std::size_t slot = 0; slot < count; ++slot) {
                if (kickHits[slot] <= 0)
                    continue;
                verlet::kick(crystal.particles[cursor + slot], kickSum[slot] / double(kickHits[slot]));
            }
            if (peakStrain <= Settings::Cohesion::breakStrain)
                return;
            for (std::size_t slot = 0; slot < count; ++slot)
                crystal.particles[cursor + slot].cohesion = 0.0f;
        }

    }

    void reconcile(const mech::Construction& construction, Crystal::Quantum& crystal) {
        if (crystal.particles.size() != construction.evaluatedParticles.size() or crystal.particles.size() != crystal.shape.size())
            return;
        const double stiff = double(Settings::constraintStiffness);
        std::size_t cursor = 0;
        mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id, const mech::Construction::Primitive& primitive) {
            restorePrimitive(crystal, primitive, cursor, stiff);
            cursor += primitive.loop.size();
        });
    }

}
