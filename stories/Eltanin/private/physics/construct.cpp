#include "physics/construct.h"
#include "physics/settings.h"
#include "physics/verlet.h"
#include "mech/construction.h"
#include "mech/semantics/space.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using rigid::Crystal;

    namespace {

        auto sameGrid(index3 left, index3 right) -> bool {
            return left.x == right.x and left.y == right.y and left.z == right.z;
        }

        auto knotComs(const Crystal::Quantum& crystal, const vector<integer>& welded, dvec3 origin, quat rotation, dvec3& liveCom, dvec3& restWorld) -> bool {
            dvec3 liveMoment{0.0, 0.0, 0.0};
            dvec3 restMoment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (const auto index : welded) {
                const auto slot = static_cast<std::size_t>(index);
                if (slot >= crystal.particles.size() or slot >= crystal.shape.size())
                    return false;
                const Particle& particle = crystal.particles[slot];
                if (particle.mass <= 0.0f)
                    continue;
                liveMoment += particle.position * double(particle.mass);
                restMoment += dvec3{crystal.shape[slot]} * double(particle.mass);
                mass += double(particle.mass);
            }
            if (mass <= 0.0)
                return false;
            liveCom = liveMoment / mass;
            restWorld = origin + glm::dquat{rotation} * (restMoment / mass);
            return true;
        }

        void spreadKnotWave(const mech::Construction& construction, Crystal::Quantum& crystal, const Body::Quantum& body) {
            struct KnotWave {
                const mech::Construction::Knot* knot;
                dvec3 restWorld;
                dvec3 delta;
            };
            vector<KnotWave> waves;
            waves.reserve(construction.knots.size());
            for (const auto& [_, knot] : construction.knots) {
                if (knot.welded.empty())
                    continue;
                dvec3 liveCom;
                dvec3 restWorld;
                if (not knotComs(crystal, knot.welded, body.position, body.orientation, liveCom, restWorld))
                    continue;
                waves.push_back(KnotWave{.knot = &knot, .restWorld = restWorld, .delta = liveCom - restWorld});
            }
            if (waves.size() < 2)
                return;
            std::size_t source = 0;
            double best = 0.0;
            for (std::size_t index = 0; index < waves.size(); ++index) {
                const double reach = glm::dot(waves[index].delta, waves[index].delta);
                if (reach > best) {
                    best = reach;
                    source = index;
                }
            }
            if (best <= double(Settings::restLinear) * double(Settings::restLinear))
                return;
            vector<char> onFrame(crystal.particles.size(), 0);
            std::size_t cursor = 0;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (construction.knots.contains(primitiveId) or construction.ribs.contains(primitiveId)) {
                    for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                        const auto particle = cursor + slot;
                        if (particle < onFrame.size())
                            onFrame[particle] = 1;
                    }
                }
                cursor += primitive.loop.size();
            });
            vector<vector<std::size_t>> next(waves.size());
            auto waveAt = [&](index3 grid) -> integer {
                for (std::size_t index = 0; index < waves.size(); ++index) {
                    if (not waves[index].knot->loop.empty() and sameGrid(waves[index].knot->loop[0].gridPos, grid))
                        return static_cast<integer>(index);
                }
                return -1;
            };
            for (const auto& [_, rib] : construction.ribs) {
                for (std::size_t slot = 1; slot < rib.loop.size(); ++slot) {
                    const auto left = waveAt(rib.loop[slot - 1].gridPos);
                    const auto right = waveAt(rib.loop[slot].gridPos);
                    if (left < 0 or right < 0)
                        continue;
                    next[static_cast<std::size_t>(left)].push_back(static_cast<std::size_t>(right));
                    next[static_cast<std::size_t>(right)].push_back(static_cast<std::size_t>(left));
                }
            }
            const dvec3 delta = waves[source].delta;
            const double span = glm::length(delta);
            const dvec3 axis = delta / span;
            const double length = double(mech::space::local::edge2meters);
            vector<double> path(waves.size(), -1.0);
            vector<std::size_t> walk;
            path[source] = 0.0;
            walk.push_back(source);
            for (std::size_t head = 0; head < walk.size(); ++head) {
                const auto from = walk[head];
                for (const auto to : next[from]) {
                    if (glm::dot(waves[to].restWorld - waves[source].restWorld, axis) <= 0.0)
                        continue;
                    const double reached = path[from] + glm::length(waves[to].restWorld - waves[from].restWorld);
                    if (path[to] >= 0.0 and path[to] <= reached)
                        continue;
                    path[to] = reached;
                    walk.push_back(to);
                }
            }
            for (std::size_t index = 0; index < waves.size(); ++index) {
                if (index == source or path[index] < 0.0)
                    continue;
                const double extra = span * std::exp(-path[index] / length) - glm::dot(waves[index].delta, axis);
                if (extra <= 0.0)
                    continue;
                const dvec3 shift = axis * extra;
                for (const auto particleIndex : waves[index].knot->welded) {
                    const auto slot = static_cast<std::size_t>(particleIndex);
                    if (slot >= crystal.particles.size() or not onFrame[slot])
                        continue;
                    verlet::semiKick(crystal.particles[slot], shift, Settings::Resilience::wave);
                }
            }
        }

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
                verlet::semiKick(crystal.particles[cursor + slot], kickSum[slot] / double(kickHits[slot]), Settings::Resilience::ribRestore);
            }
            if (peakStrain <= Settings::Cohesion::breakStrain)
                return;
            for (std::size_t slot = 0; slot < count; ++slot)
                crystal.particles[cursor + slot].cohesion = 0.0f;
        }

    }

    void reconcile(const mech::Construction& construction, Crystal::Quantum& crystal, const Body::Quantum& body) {
        if (crystal.particles.size() != construction.evaluatedParticles.size() or crystal.particles.size() != crystal.shape.size())
            return;
        spreadKnotWave(construction, crystal, body);
        const double stiff = double(Settings::constraintStiffness);
        std::size_t cursor = 0;
        mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
            if (construction.ribs.contains(primitiveId))
                restorePrimitive(crystal, primitive, cursor, stiff);
            cursor += primitive.loop.size();
        });
    }

}
