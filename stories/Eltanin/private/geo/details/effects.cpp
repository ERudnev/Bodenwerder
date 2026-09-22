#include "geo/details/effects.h"
#include "geo/details/compose.h"
#include "geo/details/facies.h"

#include <base/logging.h>

#include <algorithm>
#include <array>
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

    auto Sample::hash01(integer x, integer y, integer z, integer salt) -> float {
        std::uint32_t value = std::uint32_t(x) * 73856093u ^ std::uint32_t(y) * 19349663u ^ std::uint32_t(z) * 83492791u ^ std::uint32_t(salt) * 2654435761u;
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return float(value >> 8) * (1.0f / 16777215.0f);
    }

    auto Sample::sphereDir(integer index, integer seed, integer saltU, integer saltV) -> vec3 {
        const float u = hash01(index, seed, saltU, 11);
        const float v = hash01(index, seed, saltV, 13);
        const float theta = 2.0f * std::numbers::pi_v<float> * u;
        const float z = 2.0f * v - 1.0f;
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return glm::normalize(vec3{r * std::cos(theta), z, r * std::sin(theta)});
    }

    auto Sample::valueNoise(float x, float y, float z, integer seed) -> float {
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

    auto Sample::noise(vec3 point, integer seed) -> float {
        return valueNoise(point.x, point.y, point.z, seed) * 2.0f - 1.0f;
    }

    auto Sample::fractal(vec3 point, integer seed, integer octaves, float persistence) -> float {
        float sum = 0.0f;
        float weight = 0.0f;
        float amplitude = 1.0f;
        for (integer octave = 0; octave < octaves; ++octave) {
            sum += amplitude * noise(point, seed + octave * 19);
            weight += amplitude;
            point *= 2.07f;
            amplitude *= persistence;
        }
        return sum / std::max(weight, 1.0e-6f);
    }

    auto Sample::ridged(vec3 point, integer seed, integer octaves) -> float {
        float sum = 0.0f;
        float weight = 0.0f;
        float amplitude = 1.0f;
        float previous = 1.0f;
        for (integer octave = 0; octave < octaves; ++octave) {
            float ridge = 1.0f - std::abs(noise(point, seed + octave * 23));
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

    auto Sample::warped(vec3 direction, integer seed, float frequency, float strength) -> vec3 {
        const vec3 point = direction * frequency;
        const vec3 warp{
            Sample::fractal(point, seed + 3, 3, 0.52f),
            Sample::fractal(vec3{point.y, point.z, point.x}, seed + 7, 3, 0.52f),
            Sample::fractal(vec3{point.z, point.x, point.y}, seed + 11, 3, 0.52f),
        };
        return glm::normalize(direction + warp * strength);
    }

    auto Sample::angular(vec3 a, vec3 b) -> float {
        return std::acos(glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0f, 1.0f));
    }

    auto Sample::gaussian(float angle, float sigma) -> float {
        const float s = std::max(sigma, 1.0e-4f);
        return std::exp(-0.5f * (angle * angle) / (s * s));
    }

    auto SpatialHash::cell(vec3 direction) -> Cell {
        auto coordinate = [](float value) -> integer { return std::clamp(static_cast<integer>((value * 0.5f + 0.5f) * float(span)), integer{0}, span - 1); };
        return Cell{.x = coordinate(direction.x), .y = coordinate(direction.y), .z = coordinate(direction.z)};
    }

    auto SpatialHash::index(integer x, integer y, integer z) -> std::size_t {
        return static_cast<std::size_t>((z * span + y) * span + x);
    }

    auto PlateSite::hit(vec3 direction, const vector<PlateSite>& plates) -> Hit {
        Hit found{.first = 0, .second = 0, .firstDot = glm::dot(direction, plates[0].center), .secondDot = -2.0f};
        for (integer index = 1; index < static_cast<integer>(plates.size()); ++index) {
            const float dot = glm::dot(direction, plates[static_cast<std::size_t>(index)].center);
            if (dot > found.firstDot) {
                found.second = found.first;
                found.secondDot = found.firstDot;
                found.first = index;
                found.firstDot = dot;
            } else if (dot > found.secondDot) {
                found.second = index;
                found.secondDot = dot;
            }
        }
        return found;
    }

    auto PlateField::sample(vec3 direction) const -> Hit {
        const vec3 warped = Sample::warped(direction, seed, 2.6f, 0.34f);
        Hit found{.first = 0, .second = 0, .firstScore = -2.0f, .secondScore = -2.0f, .normal = vec3{1.0f, 0.0f, 0.0f}, .along = vec3{0.0f, 0.0f, 1.0f}, .divergence = 0.0f, .shear = 0.0f};
        for (integer index = 0; index < static_cast<integer>(sites.size()); ++index) {
            const auto& site = sites[static_cast<std::size_t>(index)];
            const float score = glm::dot(warped, site.center) + 0.09f * Sample::fractal(direction * 3.6f, seed + 101 + index * 67, 5, 0.56f) + 0.045f * Sample::fractal(direction * 10.0f, seed + 131 + index * 67, 4, 0.54f) + 0.018f * Sample::fractal(direction * 24.0f, seed + 157 + index * 67, 3, 0.52f);
            if (score > found.firstScore) {
                found.second = found.first;
                found.secondScore = found.firstScore;
                found.first = index;
                found.firstScore = score;
            } else if (score > found.secondScore) {
                found.second = index;
                found.secondScore = score;
            }
        }
        if (sites.size() < 2)
            return found;
        const auto& first = sites[static_cast<std::size_t>(found.first)];
        const auto& second = sites[static_cast<std::size_t>(found.second)];
        const vec3 projected = first.center - second.center - direction * glm::dot(first.center - second.center, direction);
        if (glm::dot(projected, projected) > 1.0e-8f)
            found.normal = glm::normalize(projected);
        vec3 along = glm::cross(direction, found.normal);
        if (glm::dot(along, along) < 1.0e-8f)
            along = glm::cross(direction, std::abs(direction.y) < 0.8f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f});
        found.along = glm::normalize(along);
        const vec3 relative = glm::cross(first.pole, direction) * first.speed - glm::cross(second.pole, direction) * second.speed;
        found.divergence = glm::dot(relative, found.normal);
        found.shear = std::abs(glm::dot(relative, found.along));
        return found;
    }

    auto Burst::impact(vec3 axis, float radius, float depth, integer seed) -> Burst {
        return Burst{.axis = axis, .radius = radius, .lift = -depth, .rim = depth * 0.18f, .seed = seed};
    }

    auto Burst::eruption(vec3 axis, float radius, float height, integer seed) -> Burst {
        return Burst{.axis = axis, .radius = radius, .lift = height, .rim = 0.0f, .seed = seed};
    }

    auto Burst::epoch(const Epoch& params) -> vector<Burst> {
        vector<Burst> bursts;
        bursts.reserve(static_cast<std::size_t>(params.count));
        for (integer crater = 0; crater < params.count; ++crater) {
            const vec3 axis = Sample::sphereDir(crater, params.seed, params.salt, params.salt + 2);
            if (axis.y > params.highland)
                continue;
            if (glm::dot(axis, params.avoidAxis) > params.avoidDot)
                continue;
            const float radius = params.radiusMin + params.radiusSpan * Sample::hash01(crater, params.seed, params.salt + 4, 17);
            const float depth = params.depthMin + params.depthSpan * Sample::hash01(crater, params.seed, params.salt + 6, 19);
            bursts.push_back(impact(axis, radius, depth, params.seed + params.salt * 997 + crater * 31));
        }
        return bursts;
    }

    auto Burst::delta(vec3 dir, const Burst& burst) -> float {
        const vec3 axis = glm::normalize(burst.axis);
        const float radius = std::max(burst.radius, 1.0e-4f);
        if (glm::dot(dir, axis) < std::cos(radius * 2.8f))
            return 0.0f;
        const vec3 warped = Sample::warped(dir, burst.seed, 2.4f, radius * 0.42f);
        const float detailFrequency = std::min(std::max(13.0f, 1.8f / radius), 280.0f);
        const float boundary = 1.0f + 0.22f * Sample::fractal(dir * detailFrequency, burst.seed + 31, 4, 0.54f);
        const float t = Sample::angular(warped, axis) / (radius * boundary);
        const float coarse = Sample::fractal(dir * std::min(std::max(9.0f, 0.9f / radius), 180.0f), burst.seed + 47, 4, 0.55f);
        const float ridges = Sample::ridged(dir * std::min(std::max(26.0f, 2.7f / radius), 360.0f), burst.seed + 71, 5);
        if (burst.lift < 0.0f) {
            const float interior = 1.0f - glm::smoothstep(0.08f, 1.0f, t);
            const float floorBreakup = interior * glm::smoothstep(0.18f, 0.86f, t) * coarse;
            const float rimDistance = (t - 1.0f) / 0.13f;
            const float brokenRim = burst.rim * std::exp(-rimDistance * rimDistance) * glm::clamp(0.58f + 0.72f * ridges + 0.22f * coarse, 0.15f, 1.5f);
            const float apron = burst.rim * 0.26f * glm::clamp(1.0f - (t - 1.0f) / 0.72f, 0.0f, 1.0f) * glm::smoothstep(0.92f, 1.06f, t) * ridges;
            return burst.lift * std::pow(interior, 0.72f) + std::abs(burst.lift) * 0.12f * floorBreakup + brokenRim + apron;
        }
        vec3 tangent = glm::cross(axis, vec3{0.0f, 1.0f, 0.0f});
        if (glm::dot(tangent, tangent) < 1.0e-8f)
            tangent = glm::cross(axis, vec3{1.0f, 0.0f, 0.0f});
        tangent = glm::normalize(tangent);
        const vec3 across = glm::normalize(glm::cross(axis, tangent));
        const float azimuth = std::atan2(glm::dot(warped, tangent), glm::dot(warped, across));
        const float lobe = 0.62f + 0.48f * Sample::fractal(vec3{std::cos(azimuth) * 2.2f, std::sin(azimuth) * 2.2f, 0.41f}, burst.seed + 91, 4, 0.56f) + 0.22f * Sample::fractal(vec3{std::cos(azimuth * 3.0f), std::sin(azimuth * 3.0f), 1.17f}, burst.seed + 97, 3, 0.52f);
        const float apron = glm::clamp(0.55f + 0.70f * ridges, 0.28f, 1.35f);
        const float radial = t / std::max(lobe * apron, 0.18f);
        const float shield = std::max(std::exp(-2.05f * radial * radial) - std::exp(-2.05f * 2.35f), 0.0f);
        const float massif = glm::clamp(0.76f + 0.22f * coarse + 0.20f * (ridges - 0.45f), 0.42f, 1.30f);
        const float ravines = std::pow(glm::clamp(ridges, 0.0f, 1.0f), 3.0f) * glm::smoothstep(0.12f, 0.82f, radial) * glm::clamp(1.15f - radial, 0.0f, 1.0f);
        const float caldera = std::exp(-std::pow(t / 0.13f, 4.0f)) * (0.11f + 0.05f * coarse);
        return burst.lift * (shield * massif - 0.16f * ravines - caldera);
    }

    void Provinces::apply(IcosaMap<float>& relief, const Provinces& params) {
        const integer count = relief.pack.storedCount();
        if (params.count <= 2) {
            for (integer index = 0; index < count; ++index) {
                base::Progress::markEvery(index);
                const auto slot = relief.pack.slotOf(index);
                const vec3 dir = relief.pack.direction(slot);
                const vec3 warped = Sample::warped(dir, params.seed, 1.35f, 0.18f);
                const float boundary = warped.y + 0.13f * Sample::fractal(dir * 3.2f, params.seed + 101, 4, 0.56f);
                const float lowland = glm::smoothstep(-0.24f, 0.24f, boundary);
                const float highland = 1.0f - glm::smoothstep(-0.32f, 0.38f, boundary);
                const float ancientMassif = 0.62f * Sample::fractal(warped * 3.6f, params.seed + 131, 6, 0.55f) + 0.38f * (Sample::ridged(warped * 7.0f, params.seed + 167, 5) - 0.46f);
                relief.at(slot) += -params.amplitude * lowland + params.amplitude * 0.72f * highland * ancientMassif;
            }
            return;
        }
        vector<vec3> sites;
        sites.reserve(static_cast<std::size_t>(params.count));
        for (integer site = 0; site < params.count; ++site)
            sites.push_back(Sample::sphereDir(site, params.seed, 3, 5));
        for (integer index = 0; index < count; ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const vec3 warped = Sample::warped(dir, params.seed, 1.7f, 0.12f);
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
            const float bestHeight = Sample::hash01(best, params.seed, 211, 17) * 2.0f - 1.0f;
            const float secondHeight = Sample::hash01(second, params.seed, 211, 17) * 2.0f - 1.0f;
            const float interior = 0.5f + 0.5f * glm::smoothstep(0.0f, 0.11f, bestDot - secondDot);
            const float province = glm::mix(secondHeight, bestHeight, interior);
            const float massif = Sample::fractal(warped * 5.0f, params.seed + best * 53, 5, 0.56f);
            relief.at(slot) += params.amplitude * (province + 0.42f * massif);
        }
    }

    void PlateField::apply(Formation& formation, const PlateField& params) {
        if (params.sites.empty())
            return;
        const float width = std::max(params.width, 0.01f);
        const float provinceScale = params.sites.size() > 1 ? 1.0f / float(params.sites.size() - 1) : 0.0f;
        for (integer index = 0; index < formation.boundary.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = formation.boundary.pack.slotOf(index);
            const vec3 direction = formation.boundary.pack.direction(slot);
            const PlateField::Hit sample = params.sample(direction);
            const float gap = (sample.firstScore - sample.secondScore) + width * (0.18f * Sample::fractal(direction * 8.5f, params.seed + 401, 5, 0.55f) + 0.08f * Sample::fractal(direction * 21.0f, params.seed + 419, 4, 0.52f));
            const float edge = params.sites.size() > 1 ? 1.0f - glm::smoothstep(width * 0.12f, width, gap) : 0.0f;
            const auto& first = params.sites[static_cast<std::size_t>(sample.first)];
            const auto& second = params.sites[static_cast<std::size_t>(sample.second)];
            const float interior = glm::smoothstep(0.0f, width, gap);
            formation.province.at(slot) = glm::mix(float(sample.first + sample.second) * 0.5f, float(sample.first), interior) * provinceScale;
            formation.composition.at(slot) = glm::mix((first.felsic + second.felsic) * 0.5f, first.felsic, interior);
            formation.crustAge.at(slot) = glm::mix((first.age + second.age) * 0.5f, first.age, interior);
            formation.boundary.at(slot) = edge * sample.divergence;
            formation.fracture.at(slot) = edge * glm::clamp(std::abs(sample.divergence) + sample.shear * 0.72f, 0.0f, 1.0f);
        }
        formation.province.stitch();
        formation.composition.stitch();
        formation.crustAge.stitch();
        formation.boundary.stitch();
        formation.fracture.stitch();
        for (integer index = 0; index < formation.relief.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = formation.relief.pack.slotOf(index);
            const vec3 direction = formation.relief.pack.direction(slot);
            const PlateField::Hit sample = params.sample(direction);
            const float gap = (sample.firstScore - sample.secondScore) + width * (0.18f * Sample::fractal(direction * 8.5f, params.seed + 401, 5, 0.55f) + 0.08f * Sample::fractal(direction * 21.0f, params.seed + 419, 4, 0.52f));
            const float edge = params.sites.size() > 1 ? 1.0f - glm::smoothstep(width * 0.12f, width, gap) : 0.0f;
            const float interior = glm::smoothstep(0.0f, width, gap);
            const auto& first = params.sites[static_cast<std::size_t>(sample.first)];
            const auto& second = params.sites[static_cast<std::size_t>(sample.second)];
            const float province = glm::mix((first.elevation + second.elevation) * 0.5f, first.elevation, interior);
            const float convergence = glm::max(-sample.divergence, 0.0f);
            const float divergence = glm::max(sample.divergence, 0.0f);
            const float mountainTexture = 0.58f + 0.64f * Sample::ridged(direction * 18.0f, params.seed + 1709, 5);
            const float shearTexture = Sample::fractal(direction * 24.0f, params.seed + 1871, 5, 0.55f);
            const float boundaryRelief = edge * params.activity * (convergence * mountainTexture * 0.20f - divergence * (0.09f + mountainTexture * 0.06f) + sample.shear * shearTexture * 0.045f);
            const float interiorTexture = Sample::fractal(Sample::warped(direction, params.seed + sample.first * 31, 2.4f, 0.08f) * 5.0f, params.seed + 1901 + sample.first * 53, 5, 0.55f);
            const float drop = first.elevation - second.elevation;
            const float highlandMask = glm::smoothstep(0.04f, 0.22f, drop) * glm::smoothstep(width * 0.06f, width * 0.40f, gap) * (1.0f - glm::smoothstep(width * 2.4f, width * 6.0f, gap));
            const float alongU = glm::dot(direction, sample.along);
            const float acrossV = glm::dot(direction, sample.normal);
            const vec3 crackDomain = sample.along * (alongU * 36.0f) + sample.normal * (acrossV * 8.5f) + direction * 1.1f;
            const float ridges = Sample::ridged(Sample::warped(crackDomain, params.seed + 2113, 1.8f, 0.12f), params.seed + 2131, 5);
            const float forks = Sample::ridged(sample.along * (alongU * 19.0f) + sample.normal * (acrossV * 24.0f), params.seed + 2149, 4);
            const float cracks = std::pow(glm::clamp(1.0f - ridges, 0.0f, 1.0f), 2.6f) * (0.50f + 0.50f * std::pow(glm::clamp(1.0f - forks, 0.0f, 1.0f), 1.8f));
            formation.relief.at(slot) += params.amplitude * (province * 0.13f + interiorTexture * (0.035f + first.age * 0.025f) + boundaryRelief - highlandMask * cracks * 0.055f * (0.55f + 0.45f * params.activity));
        }
        formation.relief.stitch();
    }

    void Basin::apply(Formation& formation, const Basin& params) {
        const vec3 center = glm::normalize(params.center);
        vec3 along = params.along - center * glm::dot(params.along, center);
        if (glm::dot(along, along) < 1.0e-8f)
            along = glm::cross(center, vec3{0.0f, 1.0f, 0.0f});
        if (glm::dot(along, along) < 1.0e-8f)
            along = glm::cross(center, vec3{1.0f, 0.0f, 0.0f});
        along = glm::normalize(along);
        const vec3 across = glm::normalize(glm::cross(center, along));
        const float radius = std::max(params.radius, 1.0e-4f);
        auto footprint = [&](vec3 direction) -> vec3 {
            const vec3 warped = Sample::warped(direction, params.seed + 11, std::min(48.0f, 3.2f / radius), radius * 0.24f);
            const float x = std::atan2(glm::dot(warped, along), glm::dot(warped, center));
            const float y = std::asin(glm::clamp(glm::dot(warped, across), -1.0f, 1.0f));
            const float bend = params.obliquity * radius * 0.18f * Sample::fractal(vec3{x / radius * 2.8f, 0.41f, 1.37f}, params.seed + 37, 4, 0.56f);
            const float longRadius = radius * (1.0f + params.obliquity * 0.62f);
            const float wideRadius = radius * (1.0f - params.obliquity * 0.24f);
            const float normalized = std::sqrt((x * x) / (longRadius * longRadius) + ((y - bend) * (y - bend)) / (wideRadius * wideRadius));
            const float broken = normalized / glm::clamp(1.0f + 0.16f * Sample::fractal(direction * std::min(180.0f, 11.0f / radius), params.seed + 71, 5, 0.58f), 0.72f, 1.28f);
            return vec3{broken, x / radius, y / radius};
        };
        for (integer index = 0; index < formation.relief.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = formation.relief.pack.slotOf(index);
            const vec3 direction = formation.relief.pack.direction(slot);
            if (glm::dot(direction, center) < std::cos(radius * 2.8f))
                continue;
            const vec3 shape = footprint(direction);
            if (shape.x > 1.65f)
                continue;
            const float cavity = 1.0f - glm::smoothstep(0.20f, 1.0f, shape.x);
            const float rim = std::exp(-std::pow((shape.x - 1.02f) / 0.18f, 2.0f));
            const float trailing = glm::smoothstep(-0.35f, 1.15f, shape.y) * params.obliquity;
            const float floor = Sample::fractal(direction * std::min(220.0f, 14.0f / radius), params.seed + 113, 5, 0.54f);
            formation.relief.at(slot) += -params.depth * cavity * (0.82f + floor * 0.18f) + params.depth * rim * (0.13f + trailing * 0.09f);
        }
        for (integer index = 0; index < formation.impact.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = formation.impact.pack.slotOf(index);
            const vec3 direction = formation.impact.pack.direction(slot);
            if (glm::dot(direction, center) < std::cos(radius * 2.8f))
                continue;
            const vec3 shape = footprint(direction);
            const float affected = 1.0f - glm::smoothstep(0.45f, 1.45f, shape.x);
            formation.impact.at(slot) = std::max(formation.impact.at(slot), affected);
            formation.exogenic.at(slot) = std::max(formation.exogenic.at(slot), affected * params.exogenic * glm::smoothstep(-0.8f, 1.2f, shape.y));
        }
        formation.relief.stitch();
        formation.impact.stitch();
        formation.exogenic.stitch();
    }

    void Volcanic::stamp(IcosaMap<float>& field, vec3 axis, float radius, float amount, integer seed) {
        axis = glm::normalize(axis);
        const float safeRadius = std::max(radius, 1.0e-4f);
        vec3 tangent = glm::cross(axis, vec3{0.0f, 1.0f, 0.0f});
        if (glm::dot(tangent, tangent) < 1.0e-8f)
            tangent = glm::cross(axis, vec3{1.0f, 0.0f, 0.0f});
        tangent = glm::normalize(tangent);
        const vec3 across = glm::normalize(glm::cross(axis, tangent));
        for (integer index = 0; index < field.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = field.pack.slotOf(index);
            const vec3 direction = field.pack.direction(slot);
            if (glm::dot(direction, axis) < std::cos(safeRadius * 3.2f))
                continue;
            const vec3 warped = Sample::warped(direction, seed, 2.7f, safeRadius * 0.55f);
            const float azimuth = std::atan2(glm::dot(warped, tangent), glm::dot(warped, across));
            const float reach = Sample::angular(warped, axis) / safeRadius;
            const float lobe = 0.42f + 0.72f * (0.5f + 0.5f * Sample::fractal(vec3{std::cos(azimuth) * 2.4f, std::sin(azimuth) * 2.4f, 0.51f}, seed + 11, 5, 0.56f)) + 0.28f * Sample::fractal(vec3{std::cos(azimuth * 5.0f), std::sin(azimuth * 5.0f), 1.23f}, seed + 19, 3, 0.52f);
            const float fingers = Sample::ridged(warped * 7.5f, seed + 29, 5);
            const float apron = glm::clamp(lobe * (0.55f + 0.70f * fingers), 0.22f, 1.55f);
            const float mask = 1.0f - glm::smoothstep(0.18f, 1.12f, reach / apron);
            if (mask <= 1.0e-4f)
                continue;
            const float core = std::exp(-2.8f * std::pow(reach / std::max(lobe, 0.18f), 2.0f));
            field.at(slot) = std::max(field.at(slot), amount * glm::max(core, mask * (0.22f + 0.78f * fingers)));
        }
        field.stitch();
    }

    void Burst::apply(IcosaMap<float>& relief, const Burst& params) {
        apply(relief, vector<Burst>{params});
    }

    void Burst::apply(IcosaMap<float>& relief, const vector<Burst>& bursts) {
        if (bursts.empty())
            return;
        const integer count = relief.pack.storedCount();
        const integer burstCount = static_cast<integer>(bursts.size());
        vector<vector<integer>> bins(static_cast<std::size_t>(SpatialHash::span * SpatialHash::span * SpatialHash::span));
        const float cell = 2.0f / float(SpatialHash::span);
        for (integer burst = 0; burst < burstCount; ++burst) {
            const float theta = std::max(bursts[static_cast<std::size_t>(burst)].radius, 1.0e-4f) * 2.8f;
            // Same cone as Burst::delta's dot test. One hash cell of slack, so a hit is never dropped; extras still return 0 and do not change the sum.
            const float chord = theta >= std::numbers::pi_v<float> ? 2.0f : 2.0f * std::sin(0.5f * theta);
            const vec3 axis = glm::normalize(bursts[static_cast<std::size_t>(burst)].axis);
            const SpatialHash::Cell first = SpatialHash::cell(axis - vec3{chord + cell});
            const SpatialHash::Cell last = SpatialHash::cell(axis + vec3{chord + cell});
            for (integer z = first.z; z <= last.z; ++z) {
                for (integer y = first.y; y <= last.y; ++y) {
                    for (integer x = first.x; x <= last.x; ++x)
                        bins[SpatialHash::index(x, y, z)].push_back(burst);
                }
            }
        }
        for (auto& bin : bins)
            std::sort(bin.begin(), bin.end());
        for (integer index = 0; index < count; ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const SpatialHash::Cell here = SpatialHash::cell(dir);
            float delta = 0.0f;
            for (integer burst : bins[SpatialHash::index(here.x, here.y, here.z)])
                delta += Burst::delta(dir, bursts[static_cast<std::size_t>(burst)]);
            relief.at(slot) += delta;
        }
    }

    void Swell::apply(IcosaMap<float>& relief, const Swell& params) {
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const vec3 warped = Sample::warped(dir, params.seed, 2.2f, params.sigma * 0.42f);
            vec3 tangent = glm::cross(glm::normalize(params.axis), vec3{0.0f, 1.0f, 0.0f});
            if (glm::dot(tangent, tangent) < 1.0e-8f)
                tangent = glm::cross(glm::normalize(params.axis), vec3{1.0f, 0.0f, 0.0f});
            tangent = glm::normalize(tangent);
            const vec3 across = glm::normalize(glm::cross(glm::normalize(params.axis), tangent));
            const float azimuth = std::atan2(glm::dot(warped, tangent), glm::dot(warped, across));
            const float lobe = 0.70f + 0.48f * Sample::fractal(vec3{std::cos(azimuth) * 1.8f, std::sin(azimuth) * 1.8f, 0.33f}, params.seed + 19, 4, 0.56f) + 0.18f * Sample::ridged(warped * 6.5f, params.seed + 29, 4);
            const float base = Sample::gaussian(Sample::angular(warped, params.axis), params.sigma * lobe);
            const float massif = 0.68f * Sample::fractal(dir * 4.2f, params.seed + 37, 5, 0.56f) + 0.32f * (Sample::ridged(dir * 9.0f, params.seed + 53, 5) - 0.45f);
            relief.at(slot) += params.amplitude * base * glm::clamp(0.88f + 0.34f * massif, 0.55f, 1.25f);
        }
    }

    void Rift::apply(IcosaMap<float>& relief, const Rift& params) {
        const integer count = relief.pack.storedCount();
        const vec3 center = glm::normalize(params.center);
        const vec3 along = glm::normalize(params.along - center * glm::dot(params.along, center));
        const vec3 across = glm::normalize(glm::cross(center, along));
        const float halfLength = std::max(params.halfLength, 1.0e-4f);
        const float halfWidth = std::max(params.halfWidth, 1.0e-4f);
        for (integer index = 0; index < count; ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const float x0 = std::atan2(glm::dot(dir, along), glm::dot(dir, center));
            const float y0 = std::asin(glm::clamp(glm::dot(dir, across), -1.0f, 1.0f));
            if (std::abs(x0) > halfLength * 1.55f or std::abs(y0) > halfWidth * 7.5f)
                continue;
            const float alongCoord = x0 / halfLength;
            const vec3 pathLow{alongCoord * 2.2f, 0.21f, 1.07f};
            const vec3 pathMid{alongCoord * 8.4f, 0.73f, 2.41f};
            const vec3 pathHigh{alongCoord * 21.0f, 1.19f, 3.83f};
            const float fold = halfLength * 0.22f * Sample::fractal(pathLow, params.seed + 7, 4, 0.62f) + halfLength * 0.08f * Sample::fractal(pathMid, params.seed + 13, 3, 0.55f);
            const float x = x0 + fold;
            const vec3 pathNow{x / halfLength * 8.4f, 0.73f, 2.41f};
            const float meander = halfWidth * (1.55f * Sample::fractal(pathLow, params.seed, 5, 0.62f) + 0.95f * Sample::fractal(pathNow, params.seed + 17, 5, 0.58f) + 0.32f * Sample::fractal(pathHigh, params.seed + 23, 4, 0.52f));
            const float widthNoise = 0.58f + 0.72f * (0.5f + 0.5f * Sample::fractal(pathNow * 1.35f, params.seed + 29, 5, 0.55f));
            const float width = halfWidth * widthNoise;
            const float raggedEnd = halfLength * (0.78f + 0.28f * Sample::fractal(dir * 8.0f, params.seed + 43, 4, 0.53f));
            const float reach = 1.0f - glm::smoothstep(raggedEnd * 0.62f, raggedEnd, std::abs(x));
            const float mainDistance = std::abs(y0 - meander);
            const float mainTrough = 1.0f - glm::smoothstep(width * 0.38f, width * 1.22f, mainDistance);
            const float branchGate = glm::smoothstep(-halfLength * 0.28f, halfLength * 0.08f, x) * (1.0f - glm::smoothstep(halfLength * 0.55f, halfLength * 0.92f, x)) * glm::smoothstep(0.12f, 0.42f, Sample::fractal(pathLow * 0.9f, params.seed + 61, 3, 0.56f));
            const float branchCenter = meander + width * (1.15f + 1.35f * Sample::fractal(pathNow * 0.7f, params.seed + 67, 4, 0.56f)) * (Sample::fractal(pathLow, params.seed + 71, 3, 0.6f) >= 0.0f ? 1.0f : -1.0f);
            const float branchTrough = (1.0f - glm::smoothstep(width * 0.24f, width * 0.78f, std::abs(y0 - branchCenter))) * branchGate;
            const float fracture = glm::clamp(0.72f + 0.38f * Sample::fractal(dir * 31.0f, params.seed + 83, 4, 0.57f), 0.38f, 1.22f);
            const float trough = std::max(mainTrough, branchTrough * 0.72f);
            const float longDepth = glm::clamp(0.38f + 0.62f * (0.5f + 0.5f * Sample::fractal(vec3{alongCoord * 1.55f, 0.47f, 2.13f}, params.seed + 97, 4, 0.66f)), 0.16f, 1.28f);
            const float sills = 0.78f + 0.22f * Sample::fractal(vec3{alongCoord * 6.2f, 1.31f, 0.91f}, params.seed + 103, 3, 0.55f);
            const float shoulder = std::exp(-std::pow((mainDistance - width * 1.25f) / std::max(width * 0.42f, 1.0e-4f), 2.0f));
            relief.at(slot) += reach * (-params.depth * longDepth * sills * std::pow(trough, 0.58f) * fracture + params.depth * 0.08f * shoulder);
        }
    }

    void Erode::apply(IcosaMap<float>& relief, const Erode& params) {
        const float force = glm::clamp(params.strength * params.years, 0.0f, 1.0f);
        if (force <= 1.0e-5f or params.iterations <= 0)
            return;
        const float step = std::acos(1.0f / std::sqrt(5.0f)) / float(std::max(relief.pack.edgeSegments(), integer{1}));
        const integer count = relief.pack.storedCount();
        IcosaMap<float> drainage{relief.pack, 0.0f};
        for (integer index = 0; index < count; ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            drainage.at(slot) = std::pow(glm::clamp(Sample::ridged(Sample::warped(dir, params.seed, 3.0f, 0.08f) * 22.0f, params.seed + 73, 5), 0.0f, 1.0f), 3.2f);
        }
        IcosaMap<float> source{relief.pack, 0.0f};
        IcosaMap<float> moved{relief.pack, 0.0f};
        for (integer iteration = 0; iteration < params.iterations; ++iteration) {
            source.values = relief.values;
            std::fill(moved.values.begin(), moved.values.end(), 0.0f);
            for (integer index = 0; index < count; ++index) {
                base::Progress::markEvery(index);
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

    void Drainage::apply(IcosaMap<float>& relief, const Drainage& params) {
        if (params.sources <= 0 or params.steps <= 0 or params.stepLength <= 0.0f or params.width <= 0.0f or params.depth <= 0.0f)
            return;
        vector<Drainage::Source> candidates;
        candidates.reserve(static_cast<std::size_t>(params.sources * 24));
        for (integer candidate = 0; candidate < params.sources * 24; ++candidate) {
            const vec3 direction = Sample::sphereDir(candidate, params.seed, 3, 5);
            if (std::abs(direction.y) < 0.82f)
                candidates.push_back(Drainage::Source{.direction = direction, .height = relief.at(direction)});
        }
        std::sort(candidates.begin(), candidates.end(), [](const Drainage::Source& a, const Drainage::Source& b) { return a.height > b.height; });
        vector<vec3> sources;
        vector<Drainage::Channel> channels;
        sources.reserve(static_cast<std::size_t>(params.sources));
        channels.reserve(static_cast<std::size_t>(params.sources * params.steps));
        for (const Drainage::Source& candidate : candidates) {
            if (static_cast<integer>(sources.size()) >= params.sources)
                break;
            bool separated = true;
            for (vec3 source : sources) {
                if (glm::dot(source, candidate.direction) > std::cos(0.16f)) {
                    separated = false;
                    break;
                }
            }
            if (not separated)
                continue;
            sources.push_back(candidate.direction);
            vec3 direction = candidate.direction;
            vec3 momentum{0.0f};
            for (integer stepIndex = 0; stepIndex < params.steps; ++stepIndex) {
                vec3 tangentA = glm::cross(vec3{0.0f, 1.0f, 0.0f}, direction);
                if (glm::dot(tangentA, tangentA) < 1.0e-8f)
                    tangentA = glm::cross(vec3{1.0f, 0.0f, 0.0f}, direction);
                tangentA = glm::normalize(tangentA);
                const vec3 tangentB = glm::cross(direction, tangentA);
                const float diagonal = params.stepLength * 0.70710678f;
                const std::array<vec3, 8> probes{
                    glm::normalize(direction + tangentA * params.stepLength), glm::normalize(direction - tangentA * params.stepLength),
                    glm::normalize(direction + tangentB * params.stepLength), glm::normalize(direction - tangentB * params.stepLength),
                    glm::normalize(direction + (tangentA + tangentB) * diagonal), glm::normalize(direction + (tangentA - tangentB) * diagonal),
                    glm::normalize(direction + (-tangentA + tangentB) * diagonal), glm::normalize(direction - (tangentA + tangentB) * diagonal),
                };
                const float currentHeight = relief.at(direction);
                float lowest = currentHeight;
                vec3 next = direction;
                for (vec3 probe : probes) {
                    const float height = relief.at(probe);
                    if (height < lowest) {
                        lowest = height;
                        next = probe;
                    }
                }
                if (lowest >= currentHeight - 0.01f)
                    break;
                vec3 heading = glm::normalize(next - direction * glm::dot(next, direction));
                if (glm::dot(momentum, momentum) > 1.0e-8f)
                    heading = glm::normalize(heading * 0.76f + momentum * 0.24f);
                const vec3 lateral = glm::normalize(glm::cross(direction, heading));
                heading = glm::normalize(heading + lateral * Sample::noise(direction * 37.0f, params.seed + stepIndex * 13 + static_cast<integer>(sources.size()) * 101) * 0.14f);
                const float maturity = std::sqrt(float(stepIndex + 1) / float(params.steps));
                channels.push_back(Drainage::Channel{.direction = direction, .width = params.width * (0.62f + 0.76f * maturity), .depth = params.depth * (0.42f + 0.72f * maturity)});
                momentum = heading;
                direction = glm::normalize(direction + heading * params.stepLength);
            }
        }
        vector<vector<integer>> bins(static_cast<std::size_t>(SpatialHash::span * SpatialHash::span * SpatialHash::span));
        for (integer point = 0; point < static_cast<integer>(channels.size()); ++point) {
            const SpatialHash::Cell cell = SpatialHash::cell(channels[static_cast<std::size_t>(point)].direction);
            bins[SpatialHash::index(cell.x, cell.y, cell.z)].push_back(point);
        }
        const float reach = params.width * 3.5f;
        for (integer index = 0; index < relief.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 direction = relief.pack.direction(slot);
            const SpatialHash::Cell first = SpatialHash::cell(direction - vec3{reach});
            const SpatialHash::Cell last = SpatialHash::cell(direction + vec3{reach});
            float cut = 0.0f;
            for (integer z = first.z; z <= last.z; ++z) {
                for (integer y = first.y; y <= last.y; ++y) {
                    for (integer x = first.x; x <= last.x; ++x) {
                        for (integer point : bins[SpatialHash::index(x, y, z)]) {
                            const Drainage::Channel& channel = channels[static_cast<std::size_t>(point)];
                            const float distance = std::sqrt(std::max(0.0f, 2.0f - 2.0f * glm::dot(direction, channel.direction)));
                            const float trough = 1.0f - glm::smoothstep(channel.width * 0.22f, channel.width * 1.18f, distance);
                            const float fracture = glm::clamp(0.82f + 0.24f * Sample::fractal(direction * 170.0f, params.seed + point * 7, 3, 0.55f), 0.55f, 1.12f);
                            cut = std::max(cut, channel.depth * trough * trough * fracture);
                        }
                    }
                }
            }
            relief.at(slot) -= cut;
        }
    }

    void Bombardment::apply(IcosaMap<float>& relief, const Bombardment& params) {
        if (params.count <= 0 or params.radiusMin <= 0.0f or params.radiusMax < params.radiusMin or params.depth <= 0.0f)
            return;
        vector<Burst> impacts;
        impacts.reserve(static_cast<std::size_t>(params.count));
        for (integer candidate = 0; candidate < params.count * 12 and static_cast<integer>(impacts.size()) < params.count; ++candidate) {
            const vec3 axis = Sample::sphereDir(candidate, params.seed, 3, 5);
            const float north = glm::smoothstep(-0.16f, 0.42f, axis.y);
            const float density = glm::mix(1.0f, glm::clamp(params.northDensity, 0.0f, 1.0f), north);
            if (Sample::hash01(candidate, params.seed, 401, 23) > density)
                continue;
            const float radiusRoll = std::pow(Sample::hash01(candidate, params.seed, 409, 29), 3.6f);
            const float radius = glm::mix(params.radiusMin, params.radiusMax, radiusRoll);
            const float size = std::sqrt(radius / params.radiusMax);
            const float depth = params.depth * size * (0.38f + 0.62f * Sample::hash01(candidate, params.seed, 419, 31));
            impacts.push_back(Burst{.axis = axis, .radius = radius, .lift = -depth, .rim = depth * 0.14f, .seed = params.seed + candidate * 43});
        }
        vector<vector<integer>> bins(static_cast<std::size_t>(SpatialHash::span * SpatialHash::span * SpatialHash::span));
        for (integer impact = 0; impact < static_cast<integer>(impacts.size()); ++impact) {
            const SpatialHash::Cell cell = SpatialHash::cell(impacts[static_cast<std::size_t>(impact)].axis);
            bins[SpatialHash::index(cell.x, cell.y, cell.z)].push_back(impact);
        }
        const float reach = params.radiusMax * 2.8f;
        for (integer index = 0; index < relief.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            const vec3 direction = relief.pack.direction(slot);
            const SpatialHash::Cell first = SpatialHash::cell(direction - vec3{reach});
            const SpatialHash::Cell last = SpatialHash::cell(direction + vec3{reach});
            float delta = 0.0f;
            for (integer z = first.z; z <= last.z; ++z) {
                for (integer y = first.y; y <= last.y; ++y) {
                    for (integer x = first.x; x <= last.x; ++x) {
                        for (integer impact : bins[SpatialHash::index(x, y, z)])
                            delta += Burst::delta(direction, impacts[static_cast<std::size_t>(impact)]);
                    }
                }
            }
            relief.at(slot) += delta;
        }
    }

    void Rub::apply(IcosaMap<float>& relief, const Rub& params) {
        const vec3 pole = glm::normalize(params.a - params.b);
        const float width = std::max(params.width, 1.0e-4f);
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = relief.pack.slotOf(index);
            const vec3 dir = relief.pack.direction(slot);
            const vec3 warped = Sample::warped(dir, params.seed, 2.0f, width * 1.8f);
            const float signedAcross = std::asin(glm::clamp(glm::dot(warped, pole), -1.0f, 1.0f));
            const float boundary = signedAcross + width * 0.45f * Sample::fractal(dir * 9.0f, params.seed + 37, 5, 0.58f);
            const float ridge = std::exp(-std::pow((boundary - width * 0.48f) / (width * 0.52f), 2.0f));
            const float trench = std::exp(-std::pow((boundary + width * 0.42f) / (width * 0.36f), 2.0f));
            const float fracture = glm::clamp(0.68f + 0.44f * Sample::ridged(dir * 19.0f, params.seed + 71, 5), 0.45f, 1.18f);
            relief.at(slot) += params.slip * (ridge * 0.72f - trench * 0.38f) * fracture;
        }
    }

    void Whisper::apply(IcosaMap<float>& relief, const Whisper& params) {
        const integer count = relief.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            base::Progress::markEvery(index);
            const auto slot = relief.pack.slotOf(index);
            relief.at(slot) += Sample::fractal(relief.pack.direction(slot) * params.freq, params.seed, 6, 0.54f) * params.amplitude;
        }
    }

    void Compose::paint(Planet& planet, const Formation& formation, const Geology& geology) {
        const integer last = formation.relief.pack.edgeSegments();
        const integer olivine = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Olivine);
        const integer pyroxene = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Pyroxene);
        const integer feldspar = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Feldspar);
        const integer clay = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Clay);
        const integer carbonaceous = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Carbonaceous);
        const integer iron = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Iron);
        const integer sulfides = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Sulfides);
        const integer oxides = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Oxides);
        const integer salts = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Salts);
        const integer waterInventory = geo::Volatile::nibble(geology.climate.retained, geo::Volatile::Kind::Water);
        const integer carbonDioxide = geo::Volatile::nibble(geology.climate.retained, geo::Volatile::Kind::CarbonDioxide);
        const integer methane = geo::Volatile::nibble(geology.climate.retained, geo::Volatile::Kind::Methane);
        const integer sulfurDioxide = geo::Volatile::nibble(geology.climate.retained, geo::Volatile::Kind::SulfurDioxide);
        auto heightAt = [&](IcosaPack::Slot slot) -> float {
            slot.iu = std::clamp(slot.iu, integer{0}, last);
            slot.iv = std::clamp(slot.iv, integer{0}, last);
            return formation.relief.at(slot);
        };
        auto pointAt = [&](IcosaPack::Slot sample) -> vec3 {
            sample.iu = std::clamp(sample.iu, integer{0}, last);
            sample.iv = std::clamp(sample.iv, integer{0}, last);
            return planet.covers.pack.direction(sample) * (planet.passport.radius + heightAt(sample));
        };
        for (integer index = 0; index < planet.covers.pack.storedCount(); ++index) {
            base::Progress::markEvery(index);
            const auto slot = planet.covers.pack.slotOf(index);
            const vec3 direction = planet.covers.pack.direction(slot);
            const float relief = formation.relief.at(slot);
            const float du = heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu + 1, .iv = slot.iv}) - heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu - 1, .iv = slot.iv});
            const float dv = heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv + 1}) - heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv - 1});
            const float slope = std::sqrt(du * du + dv * dv) / std::max(geology.history.reliefAmplitude, 1.0f);
            const IcosaPack::Tri feature = formation.volcanic.pack.triangle(IcosaPack::locate(direction));
            const float volcanic = formation.volcanic.at(feature);
            const float impact = formation.impact.at(feature);
            const float fracture = formation.fracture.at(feature);
            const float sediment = formation.sediment.at(feature);
            const float water = formation.water.at(feature);
            const float exogenic = formation.exogenic.at(feature);
            const float age = formation.crustAge.at(feature);
            const float province = formation.province.at(feature);
            const float felsic = formation.composition.at(feature);
            const float provinceTexture = Sample::fractal(Sample::warped(direction, planet.passport.seed + 6101, 2.2f, 0.11f) * (3.0f + province * 2.0f) + vec3{province * 2.7f, province * -1.9f, province * 1.3f}, planet.passport.seed + 6131, 5, 0.56f);
            const float oxidation = glm::clamp(age * 0.62f + float(oxides) / 15.0f * 0.42f + provinceTexture * 0.22f, 0.0f, 1.0f);
            Facies surface = felsic > 0.56f and feldspar > 0 ? Facies::RegolithFelsic : Facies::RegolithMafic;
            Facies below = felsic < 0.52f and pyroxene + olivine >= feldspar ? Facies::Basalt : Facies::RegolithFelsic;
            if (oxides > 0 and oxidation > 0.38f)
                surface = oxidation > 0.67f ? Facies::DesertVarnish : Facies::Hematite;
            if (feldspar > pyroxene + olivine or felsic > 0.72f)
                below = geology.crust.differentiation > 0.62f ? Facies::Granite : Facies::Anorthosite;
            if (clay > 0 and provinceTexture < -0.26f and slope < 0.05f)
                surface = Facies::ClayPan;
            if ((carbonaceous > pyroxene + feldspar and volcanic < 0.2f) or (carbonaceous > 0 and provinceTexture > 0.48f))
                surface = geology.climate.temperature < 220.0f ? Facies::Tholin : Facies::Chondrite;
            if (volcanic > 0.28f) {
                surface = age < 0.32f ? Facies::Pahoehoe : volcanic > 0.62f ? Facies::Scoria : Facies::RegolithMafic;
                below = geology.mantle.heat > 0.72f and olivine > pyroxene ? Facies::Komatiite : Facies::Basalt;
                if (sulfurDioxide + sulfides > 8 and volcanic > 0.52f)
                    surface = geology.climate.temperature < 215.0f ? Facies::SO2Frost : Facies::SulfurPlains;
            } else if (volcanic > 0.12f) {
                below = Facies::Basalt;
            }
            if (fracture > 0.42f and slope > 0.025f)
                below = geology.crust.differentiation > 0.45f and olivine > 0 ? Facies::Peridotite : Facies::Gabbro;
            if (impact > 0.18f) {
                surface = Facies::Breccia;
                below = exogenic > 0.55f and iron > 5 ? Facies::IronMetal : Facies::RegolithMafic;
            }
            if (sediment > 0.12f and slope < 0.08f) {
                if (salts > 0 and water < 0.22f)
                    surface = geology.crust.cohesion > 0.55f ? Facies::Caliche : Facies::Evaporite;
                else if (clay > 0 or geology.climate.weathering > 0.35f)
                    surface = Facies::ClayPan;
                else
                    surface = Facies::Arenite;
            }
            const float polar = std::abs(direction.y);
            float surround = relief;
            const integer rings[3] = {3, 11, 29};
            for (integer ring : rings) {
                for (integer spoke = 0; spoke < 8; ++spoke) {
                    const float angle = float(spoke) * 0.78539816f;
                    surround = std::max(surround, heightAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu + static_cast<integer>(std::lround(float(ring) * std::cos(angle))), .iv = slot.iv + static_cast<integer>(std::lround(float(ring) * std::sin(angle)))}));
                }
            }
            const float amplitude = std::max(geology.history.reliefAmplitude, 1.0f);
            const float bowl = glm::clamp((surround - relief) / amplitude, 0.0f, 1.2f);
            const vec3 east = pointAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu + 1, .iv = slot.iv}) - pointAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu - 1, .iv = slot.iv});
            const vec3 north = pointAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv + 1}) - pointAt(IcosaPack::Slot{.diamond = slot.diamond, .iu = slot.iu, .iv = slot.iv - 1});
            vec3 slopeNormal = glm::cross(east, north);
            const float slopeLength = glm::length(slopeNormal);
            if (slopeLength > 1.0e-8f) {
                slopeNormal /= slopeLength;
                if (glm::dot(slopeNormal, direction) < 0.0f)
                    slopeNormal = -slopeNormal;
            } else {
                slopeNormal = direction;
            }
            vec3 localNorth = vec3{0.0f, 1.0f, 0.0f} - direction * direction.y;
            const float northLength = glm::length(localNorth);
            localNorth = northLength > 1.0e-5f ? localNorth / northLength : vec3{1.0f, 0.0f, 0.0f};
            const vec3 tilt = slopeNormal - direction * glm::dot(slopeNormal, direction);
            const float northFacing = glm::dot(tilt, localNorth);
            const float localTemperature = geology.climate.temperature - 95.0f * polar * polar + 24.0f * (relief / amplitude) - 48.0f * bowl - 26.0f * northFacing;
            const float waterFrost = float(waterInventory) / 15.0f * (1.0f - glm::smoothstep(176.0f, 208.0f, localTemperature));
            const float carbonFrost = float(carbonDioxide) / 15.0f * (1.0f - glm::smoothstep(148.0f, 198.0f, localTemperature));
            const float methaneFrost = float(methane) / 15.0f * (1.0f - glm::smoothstep(72.0f, 112.0f, localTemperature));
            const float frostLoad = waterFrost + carbonFrost + methaneFrost;
            if (frostLoad > 0.48f) {
                const bool seasonalGas = carbonFrost + methaneFrost > waterFrost * 1.15f;
                const bool residual = frostLoad > 0.66f;
                if (seasonalGas)
                    surface = Facies::VolatileFrost;
                else if (residual)
                    surface = geology.crust.cohesion > 0.58f ? Facies::Glacier : age > 0.5f ? Facies::DirtyIce : Facies::Snow;
                else
                    surface = Facies::Snow;
                if (residual)
                    below = Facies::DirtyIce;
            }
            planet.covers.at(slot) = geo::pack(surface, below);
        }
        planet.covers.stitch();
    }

}
