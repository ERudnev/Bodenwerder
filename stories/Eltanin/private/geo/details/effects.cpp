#include "geo/celestial/generator.h"
#include "geo/details/facies.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;
    using geo::Facies;
    using geo::IcosaMap;
    using geo::IcosaPack;

    namespace {

        auto hash32(integer x, integer y, integer z, integer salt) -> std::uint32_t {
            std::uint32_t value = std::uint32_t(x) * 73856093u ^ std::uint32_t(y) * 19349663u ^ std::uint32_t(z) * 83492791u ^ std::uint32_t(salt) * 2654435761u;
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

        auto hash01(integer x, integer y, integer z, integer salt) -> float {
            return float(hash32(x, y, z, salt) >> 8) * (1.0f / 16777215.0f);
        }

        auto valueNoise(float x, float y, float z, integer seed) -> float {
            const integer x0 = static_cast<integer>(std::floor(x));
            const integer y0 = static_cast<integer>(std::floor(y));
            const integer z0 = static_cast<integer>(std::floor(z));
            const float tx = x - float(x0);
            const float ty = y - float(y0);
            const float tz = z - float(z0);
            const float sx = tx * tx * (3.0f - 2.0f * tx);
            const float sy = ty * ty * (3.0f - 2.0f * ty);
            const float sz = tz * tz * (3.0f - 2.0f * tz);
            const float c000 = hash01(x0, y0, z0, seed);
            const float c100 = hash01(x0 + 1, y0, z0, seed);
            const float c010 = hash01(x0, y0 + 1, z0, seed);
            const float c110 = hash01(x0 + 1, y0 + 1, z0, seed);
            const float c001 = hash01(x0, y0, z0 + 1, seed);
            const float c101 = hash01(x0 + 1, y0, z0 + 1, seed);
            const float c011 = hash01(x0, y0 + 1, z0 + 1, seed);
            const float c111 = hash01(x0 + 1, y0 + 1, z0 + 1, seed);
            const float c00 = c000 + (c100 - c000) * sx;
            const float c10 = c010 + (c110 - c010) * sx;
            const float c01 = c001 + (c101 - c001) * sx;
            const float c11 = c011 + (c111 - c011) * sx;
            return (c00 + (c10 - c00) * sy) * (1.0f - sz) + (c01 + (c11 - c01) * sy) * sz;
        }

        auto signedNoise(vec3 point, integer seed) -> float {
            return valueNoise(point.x, point.y, point.z, seed) * 2.0f - 1.0f;
        }

        auto fractal(vec3 point, integer seed, integer octaves, float persistence) -> float {
            float sum = 0.0f;
            float weight = 0.0f;
            float amplitude = 1.0f;
            for (integer octave = 0; octave < octaves; ++octave) {
                sum += amplitude * signedNoise(point, seed + octave * 19);
                weight += amplitude;
                point *= 2.07f;
                amplitude *= persistence;
            }
            return sum / std::max(weight, 1.0e-6f);
        }

        auto ridgedFractal(vec3 point, integer seed, integer octaves) -> float {
            float sum = 0.0f;
            float weight = 0.0f;
            float amplitude = 1.0f;
            float previous = 1.0f;
            for (integer octave = 0; octave < octaves; ++octave) {
                float ridge = 1.0f - std::abs(signedNoise(point, seed + octave * 23));
                ridge *= ridge;
                ridge *= previous;
                previous = glm::clamp(ridge * 2.0f, 0.0f, 1.0f);
                sum += amplitude * ridge;
                weight += amplitude;
                point *= 2.13f;
                amplitude *= 0.52f;
            }
            return sum / std::max(weight, 1.0e-6f);
        }

        auto warpedDirection(vec3 direction, integer seed, float frequency, float strength) -> vec3 {
            const vec3 point = direction * frequency;
            const vec3 warp{
                fractal(point, seed + 3, 3, 0.52f),
                fractal(vec3{point.y, point.z, point.x}, seed + 7, 3, 0.52f),
                fractal(vec3{point.z, point.x, point.y}, seed + 11, 3, 0.52f),
            };
            return glm::normalize(direction + warp * strength);
        }

        auto packLayers(Facies surface, Facies below) -> std::uint16_t {
            return std::uint16_t(std::uint16_t(surface) | (std::uint16_t(below) << 8));
        }

        auto angular(vec3 a, vec3 b) -> float {
            return std::acos(glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0f, 1.0f));
        }

        auto gaussian(float angle, float sigma) -> float {
            const float s = std::max(sigma, 1.0e-4f);
            return std::exp(-0.5f * (angle * angle) / (s * s));
        }

        auto sphereSite(integer index, integer seed) -> vec3 {
            const float u = hash01(index, seed, 3, 11);
            const float v = hash01(index, seed, 5, 13);
            const float theta = 2.0f * std::numbers::pi_v<float> * u;
            const float z = 2.0f * v - 1.0f;
            const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
            return glm::normalize(vec3{r * std::cos(theta), z, r * std::sin(theta)});
        }

        auto burstDelta(vec3 dir, const Generator::Burst& burst) -> float {
            const vec3 axis = glm::normalize(burst.axis);
            const float radius = std::max(burst.radius, 1.0e-4f);
            if (glm::dot(dir, axis) < std::cos(radius * 2.8f))
                return 0.0f;
            const vec3 warped = warpedDirection(dir, burst.seed, 2.4f, radius * 0.42f);
            const float boundary = 1.0f + 0.22f * fractal(dir * 13.0f, burst.seed + 31, 4, 0.54f);
            const float t = angular(warped, axis) / (radius * boundary);
            const float coarse = fractal(dir * 9.0f, burst.seed + 47, 4, 0.55f);
            const float ridges = ridgedFractal(dir * 26.0f, burst.seed + 71, 5);
            if (burst.lift < 0.0f) {
                const float interior = 1.0f - glm::smoothstep(0.08f, 1.0f, t);
                const float floorBreakup = interior * glm::smoothstep(0.18f, 0.86f, t) * coarse;
                const float rimDistance = (t - 1.0f) / 0.13f;
                const float brokenRim = burst.rim * std::exp(-rimDistance * rimDistance) * glm::clamp(0.58f + 0.72f * ridges + 0.22f * coarse, 0.15f, 1.5f);
                const float apron = burst.rim * 0.26f * glm::clamp(1.0f - (t - 1.0f) / 0.72f, 0.0f, 1.0f) * glm::smoothstep(0.92f, 1.06f, t) * ridges;
                return burst.lift * std::pow(interior, 0.72f) + std::abs(burst.lift) * 0.12f * floorBreakup + brokenRim + apron;
            }
            const float shield = std::max(std::exp(-2.3f * t * t) - std::exp(-2.3f * 2.25f), 0.0f);
            const float massif = glm::clamp(0.76f + 0.22f * coarse + 0.20f * (ridges - 0.45f), 0.42f, 1.30f);
            const float ravines = std::pow(glm::clamp(ridges, 0.0f, 1.0f), 3.0f) * glm::smoothstep(0.12f, 0.82f, t) * glm::clamp(1.15f - t, 0.0f, 1.0f);
            const float caldera = std::exp(-std::pow(t / 0.13f, 4.0f)) * (0.11f + 0.05f * coarse);
            return burst.lift * (shield * massif - 0.16f * ravines - caldera);
        }

    }

    void Generator::applyProvinces(IcosaMap<float>& relief, const Provinces& params) {
        const integer count = relief.pack.storedCount();
        if (params.count <= 2) {
            for (integer index = 0; index < count; ++index) {
                const auto slot = relief.pack.slotOf(index);
                const vec3 dir = relief.pack.direction(slot);
                const vec3 warped = warpedDirection(dir, params.seed, 1.35f, 0.18f);
                const float boundary = warped.y + 0.13f * fractal(dir * 3.2f, params.seed + 101, 4, 0.56f);
                const float lowland = glm::smoothstep(-0.24f, 0.24f, boundary);
                const float highland = 1.0f - glm::smoothstep(-0.32f, 0.38f, boundary);
                const float ancientMassif = 0.62f * fractal(warped * 3.6f, params.seed + 131, 6, 0.55f) + 0.38f * (ridgedFractal(warped * 7.0f, params.seed + 167, 5) - 0.46f);
                relief.at(slot) += -params.amplitude * lowland + params.amplitude * 0.72f * highland * ancientMassif;
            }
            return;
        }
        vector<vec3> sites;
        sites.reserve(static_cast<std::size_t>(params.count));
        for (integer site = 0; site < params.count; ++site)
            sites.push_back(sphereSite(site, params.seed));
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const vec3 warped = warpedDirection(dir, params.seed, 1.7f, 0.12f);
            integer best = 0;
            integer second = 0;
            float bestDot = glm::dot(warped, sites[0]);
            float secondDot = -2.0f;
            for (integer site = 1; site < params.count; ++site) {
                const float d = glm::dot(warped, sites[static_cast<std::size_t>(site)]);
                if (d > bestDot) {
                    secondDot = bestDot;
                    second = best;
                    bestDot = d;
                    best = site;
                } else if (d > secondDot) {
                    secondDot = d;
                    second = site;
                }
            }
            const float bestHeight = hash01(best, params.seed, 211, 17) * 2.0f - 1.0f;
            const float secondHeight = hash01(second, params.seed, 211, 17) * 2.0f - 1.0f;
            const float interior = 0.5f + 0.5f * glm::smoothstep(0.0f, 0.11f, bestDot - secondDot);
            const float province = glm::mix(secondHeight, bestHeight, interior);
            const float massif = fractal(warped * 5.0f, params.seed + best * 53, 5, 0.56f);
            relief.at(slot) += params.amplitude * (province + 0.42f * massif);
        }
    }

    void Generator::applyBurst(IcosaMap<float>& relief, const Burst& params) {
        applyBursts(relief, vector<Burst>{params});
    }

    void Generator::applyBursts(IcosaMap<float>& relief, const vector<Burst>& bursts) {
        if (bursts.empty())
            return;
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            float delta = 0.0f;
            for (const auto& burst : bursts)
                delta += burstDelta(dir, burst);
            relief.at(slot) += delta;
        }
    }

    void Generator::applySwell(IcosaMap<float>& relief, const Swell& params) {
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const vec3 warped = warpedDirection(dir, params.seed, 1.8f, params.sigma * 0.20f);
            const float base = gaussian(angular(warped, params.axis), params.sigma);
            const float massif = 0.68f * fractal(dir * 4.2f, params.seed + 37, 5, 0.56f) + 0.32f * (ridgedFractal(dir * 9.0f, params.seed + 53, 5) - 0.45f);
            relief.at(slot) += params.amplitude * base * glm::clamp(0.88f + 0.34f * massif, 0.55f, 1.25f);
        }
    }

    void Generator::applyRift(IcosaMap<float>& relief, const Rift& params) {
        const integer count = relief.pack.storedCount();
        const vec3 center = glm::normalize(params.center);
        const vec3 along = glm::normalize(params.along - center * glm::dot(params.along, center));
        const vec3 across = glm::normalize(glm::cross(center, along));
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const float x = std::atan2(glm::dot(dir, along), glm::dot(dir, center));
            const float y = std::asin(glm::clamp(glm::dot(dir, across), -1.0f, 1.0f));
            if (std::abs(x) > params.halfLength * 1.25f or std::abs(y) > params.halfWidth * 4.0f)
                continue;
            const vec3 pathPoint{x * 7.0f / std::max(params.halfLength, 1.0e-4f), 0.37f, 1.91f};
            const float meander = params.halfWidth * 0.82f * fractal(pathPoint, params.seed, 5, 0.58f);
            const float widthNoise = 0.72f + 0.58f * (0.5f + 0.5f * fractal(pathPoint * 1.7f, params.seed + 29, 4, 0.55f));
            const float width = params.halfWidth * widthNoise;
            const float raggedEnd = params.halfLength * (0.82f + 0.24f * fractal(dir * 8.0f, params.seed + 43, 4, 0.53f));
            const float reach = 1.0f - glm::smoothstep(raggedEnd * 0.72f, raggedEnd, std::abs(x));
            const float mainDistance = std::abs(y - meander);
            const float mainTrough = 1.0f - glm::smoothstep(width * 0.42f, width * 1.18f, mainDistance);
            const float branchGate = glm::smoothstep(-params.halfLength * 0.20f, params.halfLength * 0.18f, x) * (1.0f - glm::smoothstep(params.halfLength * 0.62f, params.halfLength * 0.92f, x));
            const float branchCenter = meander + width * (1.35f + 0.75f * fractal(pathPoint * 0.8f, params.seed + 61, 3, 0.56f)) * branchGate;
            const float branchTrough = (1.0f - glm::smoothstep(width * 0.28f, width * 0.72f, std::abs(y - branchCenter))) * branchGate;
            const float fracture = glm::clamp(0.78f + 0.30f * fractal(dir * 31.0f, params.seed + 83, 4, 0.57f), 0.42f, 1.18f);
            const float trough = std::max(mainTrough, branchTrough * 0.68f);
            const float shoulder = std::exp(-std::pow((mainDistance - width * 1.25f) / std::max(width * 0.42f, 1.0e-4f), 2.0f));
            relief.at(slot) += reach * (-params.depth * std::pow(trough, 0.58f) * fracture + params.depth * 0.08f * shoulder);
        }
    }

    void Generator::applyErode(IcosaMap<float>& relief, const Erode& params) {
        const float force = glm::clamp(params.strength * params.years, 0.0f, 1.0f);
        if (force <= 1.0e-5f or params.iterations <= 0)
            return;
        const float step = std::acos(1.0f / std::sqrt(5.0f)) / float(std::max(relief.pack.edgeSegments(), integer{1}));
        const integer count = relief.pack.storedCount();
        IcosaMap<float> drainage{relief.pack, 0.0f};
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            drainage.at(slot) = std::pow(glm::clamp(ridgedFractal(warpedDirection(dir, params.seed, 3.0f, 0.08f) * 22.0f, params.seed + 73, 5), 0.0f, 1.0f), 3.2f);
        }
        for (integer iteration = 0; iteration < params.iterations; ++iteration) {
            IcosaMap<float> source{relief.pack, 0.0f};
            source.values = relief.values;
            IcosaMap<float> moved{relief.pack, 0.0f};
            for (integer index = 0; index < count; ++index) {
                const auto slot = relief.pack.slotOf(index);
                const vec3 dir = relief.pack.direction(slot);
                vec3 tangentA = glm::cross(vec3{0.0f, 1.0f, 0.0f}, dir);
                if (glm::dot(tangentA, tangentA) < 1.0e-8f)
                    tangentA = glm::cross(vec3{1.0f, 0.0f, 0.0f}, dir);
                tangentA = glm::normalize(tangentA);
                const vec3 tangentB = glm::cross(dir, tangentA);
                const float centerHeight = source.at(slot);
                const float east = source.at(glm::normalize(dir + tangentA * step));
                const float west = source.at(glm::normalize(dir - tangentA * step));
                const float north = source.at(glm::normalize(dir + tangentB * step));
                const float south = source.at(glm::normalize(dir - tangentB * step));
                float lowest = east;
                vec3 downhill = glm::normalize(dir + tangentA * step);
                if (west < lowest) {
                    lowest = west;
                    downhill = glm::normalize(dir - tangentA * step);
                }
                if (north < lowest) {
                    lowest = north;
                    downhill = glm::normalize(dir + tangentB * step);
                }
                if (south < lowest) {
                    lowest = south;
                    downhill = glm::normalize(dir - tangentB * step);
                }
                const float slopeDrop = std::max(centerHeight - lowest, 0.0f);
                const float northGate = glm::smoothstep(-0.08f, 0.42f, dir.y);
                const float mask = glm::mix(1.0f, northGate, glm::clamp(params.north, 0.0f, 1.0f));
                const float channel = drainage.at(slot);
                const float sediment = slopeDrop * force * mask * (0.012f + 0.072f * channel);
                moved.at(slot) -= sediment;
                const IcosaPack::Tri target = relief.pack.triangle(IcosaPack::locate(downhill));
                moved.at(target.a) += sediment * target.bary.x;
                moved.at(target.b) += sediment * target.bary.y;
                moved.at(target.c) += sediment * target.bary.z;
            }
            for (integer index = 0; index < count; ++index)
                relief.values[static_cast<std::size_t>(index)] = source.values[static_cast<std::size_t>(index)] + moved.values[static_cast<std::size_t>(index)];
            relief.stitch();
        }
    }

    void Generator::applyRub(IcosaMap<float>& relief, const Rub& params) {
        const vec3 pole = glm::normalize(params.a - params.b);
        const float width = std::max(params.width, 1.0e-4f);
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const vec3 warped = warpedDirection(dir, params.seed, 2.0f, width * 1.8f);
            const float signedAcross = std::asin(glm::clamp(glm::dot(warped, pole), -1.0f, 1.0f));
            const float boundary = signedAcross + width * 0.45f * fractal(dir * 9.0f, params.seed + 37, 5, 0.58f);
            const float ridge = std::exp(-std::pow((boundary - width * 0.48f) / (width * 0.52f), 2.0f));
            const float trench = std::exp(-std::pow((boundary + width * 0.42f) / (width * 0.36f), 2.0f));
            const float fracture = glm::clamp(0.68f + 0.44f * ridgedFractal(dir * 19.0f, params.seed + 71, 5), 0.45f, 1.18f);
            relief.at(slot) += params.slip * (ridge * 0.72f - trench * 0.38f) * fracture;
        }
    }

    void Generator::applyWhisper(IcosaMap<float>& relief, const Whisper& params) {
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            relief.at(slot) += fractal(relief.pack.direction(slot) * params.freq, params.seed, 6, 0.54f) * params.amplitude;
        }
    }

    void Generator::paintCover(Planet& planet, const PaintCover& params) {
        const vec3 canyonCenter = glm::normalize(params.canyonCenter);
        const vec3 canyonAlong = glm::normalize(params.canyonAlong - canyonCenter * glm::dot(params.canyonAlong, canyonCenter));
        const vec3 canyonAcross = glm::normalize(glm::cross(canyonCenter, canyonAlong));
        const integer last = planet.heights.pack.edgeSegments();
        const integer count = planet.covers.pack.storedCount();
        auto heightAt = [&](IcosaPack::Slot slot) -> float {
            slot.iu = std::clamp(slot.iu, integer{0}, last);
            slot.iv = std::clamp(slot.iv, integer{0}, last);
            return float(planet.heights.at(slot));
        };
        for (integer index = 0; index < count; ++index) {
            const auto slot = planet.covers.pack.slotOf(index);
            const vec3 dir = planet.covers.pack.direction(slot);
            const float relief = float(planet.heights.at(slot));
            const float du = heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu + 1, .iv = slot.iv}) - heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu - 1, .iv = slot.iv});
            const float dv = heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv + 1}) - heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv - 1});
            const float slope = std::sqrt(du * du + dv * dv) / float(std::max(Planet::reliefPeak, std::int16_t{1}));
            const float polar = std::abs(dir.y);
            const float cap = 0.62f + 0.12f * (params.ice / 15.0f) + 0.06f * params.age;
            const float climate = polar + 0.085f * fractal(warpedDirection(dir, params.seed + 701, 2.1f, 0.09f) * 5.0f, params.seed + 719, 5, 0.56f) + 0.035f * relief / float(Planet::reliefPeak);
            const vec3 volcanicDir = warpedDirection(dir, params.seed + 503, 2.4f, 0.055f);
            const float shield = (gaussian(angular(volcanicDir, params.olympus), 0.17f) + gaussian(angular(volcanicDir, params.tharsis), 0.31f)) * glm::clamp(0.84f + 0.32f * fractal(dir * 14.0f, params.seed + 541, 4, 0.54f), 0.48f, 1.22f);
            const float canyonX = std::atan2(glm::dot(dir, canyonAlong), glm::dot(dir, canyonCenter));
            const float canyonY = std::asin(glm::clamp(glm::dot(dir, canyonAcross), -1.0f, 1.0f));
            const float canyonMeander = params.canyonHalfWidth * 0.82f * fractal(vec3{canyonX * 7.0f / std::max(params.canyonHalfLength, 1.0e-4f), 0.37f, 1.91f}, params.seed + 401, 5, 0.58f);
            const bool inCanyon = std::abs(canyonX) < params.canyonHalfLength * 1.05f and std::abs(canyonY - canyonMeander) < params.canyonHalfWidth * 1.8f and relief < -0.06f * float(Planet::reliefPeak);
            const bool inCrater = relief < -0.16f * float(Planet::reliefPeak) and shield < 0.30f;
            Facies surface = Facies::RegolithMafic;
            Facies below = Facies::Basalt;
            if (params.oxides > 0)
                surface = params.cohesion < 0.45f ? Facies::Hematite : Facies::DesertVarnish;
            if (params.clay > params.oxides and params.clay > 0)
                surface = Facies::ClayPan;
            if (params.pyroxene > 0)
                below = params.cohesion < 0.4f ? Facies::RegolithMafic : Facies::Basalt;
            if (params.feldspar > params.pyroxene and params.feldspar > 0)
                below = Facies::RegolithFelsic;
            if (params.carbonaceous > 6 and polar < 0.4f)
                surface = Facies::Chondrite;
            if (params.salts > 0 and inCrater and polar > 0.35f)
                surface = params.cohesion < 0.5f ? Facies::Evaporite : Facies::Caliche;
            if (params.iron > 8 and inCrater)
                below = Facies::IronMetal;
            if (shield > 0.55f and params.pyroxene > 0) {
                surface = params.age < 0.35f ? Facies::Pahoehoe : Facies::Scoria;
                below = Facies::Basalt;
            }
            if (inCanyon) {
                surface = slope > 0.08f ? Facies::Gabbro : Facies::Basalt;
                below = params.differentiation > 0.35f and params.olivine > 0 ? Facies::Peridotite : Facies::Gabbro;
            }
            if (inCrater and params.feldspar + params.pyroxene > 0) {
                surface = Facies::Breccia;
                below = params.iron > 8 ? Facies::IronMetal : Facies::RegolithMafic;
            }
            if (params.ice > 0 and climate > cap) {
                if (params.age > 0.65f and params.cohesion > 0.55f)
                    surface = Facies::Glacier;
                else if (params.cohesion < 0.35f)
                    surface = Facies::Snow;
                else
                    surface = Facies::DirtyIce;
                below = Facies::DirtyIce;
            }
            planet.covers.at(slot) = packLayers(surface, below);
        }
    }

}
