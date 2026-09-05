#include "physics/construct.h"
#include "physics/settings.h"
#include "physics/verlet.h"
#include "mech/construction.h"
#include "mech/semantics/space.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eltanin::phys {

    using namespace fqsm::api;
    using rigid::Crystal;

    namespace {

        auto packGrid(index3 grid) -> uint64_t {
            const auto axis = [](integer value) -> uint64_t {
                return static_cast<uint64_t>(static_cast<uint32_t>(value) + 0x100000u) & 0x1FFFFFu;
            };
            return (axis(grid.x) << 42) | (axis(grid.y) << 21) | axis(grid.z);
        }

        auto particlesMoving(const Crystal::Quantum& crystal) -> bool {
            const double rest2 = double(Settings::restLinear) * double(Settings::restLinear);
            for (const Particle& particle : crystal.particles) {
                const dvec3 step = particle.position - particle.prev;
                if (glm::dot(step, step) > rest2)
                    return true;
            }
            return false;
        }

        auto markOnFrame(const mech::Construction& construction, std::size_t particleCount) -> vector<char> {
            vector<char> onFrame(particleCount, 0);
            std::size_t cursor = 0;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (construction.knots.contains(primitiveId) or construction.ribs.contains(primitiveId) or construction.membranes.contains(primitiveId)) {
                    for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                        const auto particle = cursor + slot;
                        if (particle < onFrame.size())
                            onFrame[particle] = 1;
                    }
                }
                cursor += primitive.loop.size();
            });
            return onFrame;
        }

        auto knotComs(const Crystal::Quantum& crystal, const vector<integer>& welded, const vector<char>& onFrame, dvec3 origin, quat rotation, dvec3& liveCom, dvec3& restWorld) -> bool {
            dvec3 liveMoment{0.0, 0.0, 0.0};
            dvec3 restMoment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (const auto index : welded) {
                const auto slot = static_cast<std::size_t>(index);
                if (slot >= crystal.particles.size() or slot >= crystal.shape.size())
                    return false;
                if (slot >= onFrame.size() or not onFrame[slot])
                    continue;
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

        void spreadKnotWave(const mech::Construction& construction, Crystal::Quantum& crystal, const Body::Quantum& body, const vector<char>& onFrame) {
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
                if (not knotComs(crystal, knot.welded, onFrame, body.position, body.orientation, liveCom, restWorld))
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
            vector<vector<std::size_t>> next(waves.size());
            std::unordered_map<uint64_t, std::size_t> waveAtGrid;
            waveAtGrid.reserve(waves.size());
            for (std::size_t index = 0; index < waves.size(); ++index) {
                if (waves[index].knot->loop.empty())
                    continue;
                waveAtGrid.emplace(packGrid(waves[index].knot->loop[0].gridPos), index);
            }
            auto waveAt = [&](index3 grid) -> integer {
                const auto found = waveAtGrid.find(packGrid(grid));
                if (found == waveAtGrid.end())
                    return -1;
                return static_cast<integer>(found->second);
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

        void peelSkin(const mech::Construction& construction, Crystal::Quantum& crystal, const Body::Quantum& body, const vector<char>& onFrame) {
            std::unordered_map<uint64_t, dvec3> frameComAt;
            frameComAt.reserve(construction.knots.size());
            for (const auto& [_, knot] : construction.knots) {
                if (knot.loop.empty() or knot.welded.empty())
                    continue;
                dvec3 liveCom;
                dvec3 restWorld;
                if (not knotComs(crystal, knot.welded, onFrame, body.position, body.orientation, liveCom, restWorld))
                    continue;
                frameComAt.emplace(packGrid(knot.loop[0].gridPos), liveCom);
            }
            if (frameComAt.empty())
                return;
            auto comAt = [&](index3 grid) -> const dvec3* {
                const auto found = frameComAt.find(packGrid(grid));
                if (found == frameComAt.end())
                    return nullptr;
                return &found->second;
            };
            std::unordered_map<mech::Construction::Primitive::Id, char> peeled;
            std::size_t cursor = 0;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                const bool plate = construction.plates.contains(primitiveId);
                const bool volume = construction.volumes.contains(primitiveId);
                if (plate or volume) {
                    dvec3 worldNail{0.0, 0.0, 0.0};
                    if (plate) {
                        const vec3 local = mech::plateOutward(primitive, construction);
                        if (glm::dot(local, local) > 1.0e-8f)
                            worldNail = glm::dquat{body.orientation} * dvec3{local};
                    }
                    const double nailLen = glm::length(worldNail);
                    const dvec3 nail = nailLen > 1.0e-12 ? worldNail / nailLen : dvec3{0.0, 0.0, 0.0};
                    for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                        const auto particle = cursor + slot;
                        if (particle >= crystal.particles.size())
                            break;
                        const dvec3* liveCom = comAt(primitive.loop[slot].gridPos);
                        if (not liveCom)
                            continue;
                        const dvec3 rel = crystal.particles[particle].position - *liveCom;
                        const float slack = (plate ? Settings::Cohesion::peelPlate : Settings::Cohesion::peelMount) * primitive.loop[slot].strength;
                        const double reach = nailLen > 1.0e-12 ? glm::dot(rel, nail) : glm::length(rel);
                        if (reach > double(slack)) {
                            peeled[primitiveId] = 1;
                            break;
                        }
                    }
                }
                cursor += primitive.loop.size();
            });
            if (peeled.empty())
                return;
            cursor = 0;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (peeled.contains(primitiveId)) {
                    for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                        const auto particle = cursor + slot;
                        if (particle < crystal.particles.size())
                            crystal.particles[particle].cohesion = 0.0f;
                    }
                    crystal.visualHurtStale = true;
                }
                cursor += primitive.loop.size();
            });
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
            crystal.visualHurtStale = true;
        }

    }

    void reconcile(const mech::Construction& construction, Crystal::Quantum& crystal, const Body::Quantum& body) {
        if (crystal.particles.size() != construction.evaluatedParticles.size() or crystal.particles.size() != crystal.shape.size())
            return;
        if (not particlesMoving(crystal))
            return;
        const auto onFrame = markOnFrame(construction, crystal.particles.size());
        if (Settings::knotWave)
            spreadKnotWave(construction, crystal, body, onFrame);
        const double stiff = double(Settings::constraintStiffness);
        std::size_t cursor = 0;
        mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
            if (construction.ribs.contains(primitiveId))
                restorePrimitive(crystal, primitive, cursor, stiff);
            cursor += primitive.loop.size();
        });
        if (Settings::peelSkin)
            peelSkin(construction, crystal, body, onFrame);
    }

}
