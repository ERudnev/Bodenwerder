#include "geo/details/compose.h"
#include "geo/celestial/generator.h"

#include <eltanin/geo/minerals.q1.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    Formation::Formation(geo::IcosaPack surface, geo::IcosaPack features)
        : relief{surface, 0.0f}
        , province{features, 0.0f}
        , composition{features, 0.0f}
        , crustAge{features, 0.0f}
        , boundary{features, 0.0f}
        , volcanic{features, 0.0f}
        , impact{features, 0.0f}
        , fracture{features, 0.0f}
        , sediment{features, 0.0f}
        , water{features, 0.0f}
        , exogenic{features, 0.0f} {
    }

    auto AtmosphereLook::of(const Geology& geology) -> AtmosphereLook {
        const auto& volatiles = geo::Volatile::table();
        const auto& minerals = geo::Mineral::table();
        vec3 gasScatter{0.0f};
        vec3 gasAbsorb{0.0f};
        float retainedSum = 0.0f;
        float rayleighMass = 0.0f;
        float hazeMass = 0.0f;
        for (integer index = 0; index < 8; ++index) {
            const auto kind = static_cast<geo::Volatile::Kind>(index);
            const float amount = float(geo::Volatile::nibble(geology.climate.retained, kind));
            if (amount <= 0.0f)
                continue;
            const auto& gas = volatiles[static_cast<std::size_t>(index)];
            gasScatter += amount * gas.scatter;
            gasAbsorb += amount * gas.absorb;
            retainedSum += amount;
            if (kind == geo::Volatile::Kind::Methane or kind == geo::Volatile::Kind::Ammonia or kind == geo::Volatile::Kind::SulfurDioxide)
                hazeMass += amount;
            else
                rayleighMass += amount;
        }
        if (retainedSum > 0.0f) {
            gasScatter /= retainedSum;
            gasAbsorb /= retainedSum;
        } else {
            gasScatter = vec3{0.45f, 0.72f, 1.10f};
        }
        const float column = glm::clamp(geology.climate.atmosphere, 0.0f, 1.0f);
        const float rayleighShare = rayleighMass / std::max(retainedSum, 1.0f);
        const float hazeShare = hazeMass / std::max(retainedSum, 1.0f);
        const float oxides = float(geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Oxides)) / 15.0f;
        const float clay = float(geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Clay)) / 15.0f;
        const float soot = float(geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Carbonaceous)) / 15.0f;
        const float sulfides = float(geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Sulfides)) / 15.0f;
        const float salts = float(geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Salts)) / 15.0f;
        const float ice = float(geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Ice)) / 15.0f;
        vec3 dustColor = minerals[static_cast<std::size_t>(geo::Mineral::Kind::Oxides)].albedo * oxides;
        dustColor += minerals[static_cast<std::size_t>(geo::Mineral::Kind::Clay)].albedo * clay * 0.55f;
        dustColor += minerals[static_cast<std::size_t>(geo::Mineral::Kind::Carbonaceous)].albedo * soot * 0.40f;
        dustColor += minerals[static_cast<std::size_t>(geo::Mineral::Kind::Sulfides)].albedo * sulfides * 0.35f;
        dustColor += minerals[static_cast<std::size_t>(geo::Mineral::Kind::Salts)].albedo * salts * 0.20f;
        dustColor += minerals[static_cast<std::size_t>(geo::Mineral::Kind::Ice)].albedo * ice * 0.08f;
        const float dustLoad = oxides + clay * 0.55f + soot * 0.40f + sulfides * 0.35f + salts * 0.20f + ice * 0.08f;
        if (dustLoad > 1.0e-5f)
            dustColor /= dustLoad;
        else
            dustColor = vec3{0.55f, 0.48f, 0.40f};
        const float dustMix = glm::clamp(dustLoad / std::max(dustLoad + rayleighShare * column * 1.4f, 0.05f), 0.0f, 0.88f);
        const vec3 scatter = glm::mix(gasScatter, dustColor, dustMix);
        const vec3 day = glm::clamp(scatter * (vec3{1.0f} - gasAbsorb * 0.55f), vec3{0.04f}, vec3{1.85f});
        const float zenithTau = column > 0.0f ? glm::clamp(column * (0.18f + 0.12f * rayleighShare + 0.70f * hazeShare) + column * dustLoad * 0.08f, 0.0f, 1.25f) : 0.0f;
        return AtmosphereLook{.day = RGB{day.x, day.y, day.z}, .zenithTau = zenithTau};
    }

    auto PaintCover::mars(const Geology& geology, integer seed) -> PaintCover {
        const float differentiation = glm::clamp(geology.crust.differentiation, 0.0f, 1.0f);
        const vec3 tharsis = glm::normalize(vec3{0.72f, 0.12f, 0.35f});
        const vec3 olympus = glm::normalize(tharsis + vec3{0.04f, 0.08f, -0.02f});
        const vec3 canyonCenter = glm::normalize(tharsis + vec3{0.35f, -0.08f, -0.22f});
        const vec3 canyonAlong = glm::normalize(glm::cross(vec3{0.0f, 1.0f, 0.0f}, canyonCenter));
        return PaintCover{
            .ice = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Ice),
            .olivine = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Olivine),
            .pyroxene = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Pyroxene),
            .feldspar = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Feldspar),
            .clay = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Clay),
            .carbonaceous = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Carbonaceous),
            .iron = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Iron),
            .oxides = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Oxides),
            .salts = geo::Mineral::nibble(geology.crust.mix, geo::Mineral::Kind::Salts),
            .cohesion = glm::clamp(geology.crust.cohesion, 0.0f, 1.0f),
            .age = glm::clamp(geology.history.surfaceAge, 0.0f, 1.0f),
            .differentiation = differentiation,
            .tharsis = tharsis,
            .olympus = olympus,
            .canyonCenter = canyonCenter,
            .canyonAlong = canyonAlong,
            .canyonHalfWidth = 0.04f + 0.02f * differentiation,
            .canyonHalfLength = 0.46f,
            .seed = seed,
        };
    }

    auto Compose::derive(const Passport& passport) -> Geology {
        constexpr double gravityConstant = 6.67430e-11;
        const double radius = std::max(double(passport.radius), 1.0);
        const float gravity = float(gravityConstant * std::max(passport.mass, 0.0) / (radius * radius));
        const float age = glm::clamp(passport.ageGyr / 8.0f, 0.0f, 1.0f);
        const float stellarFlux = std::max(passport.environment.stellarFlux, 0.0f);
        const float equilibrium = 278.5f * std::pow(std::max(stellarFlux, 0.25f) / 1361.0f, 0.25f);
        float solidWeight = 0.0f;
        float solidDensity = 0.0f;
        const auto& minerals = geo::Mineral::table();
        for (integer index = 0; index < 16; ++index) {
            const float weight = float((passport.bulk >> (index * 4)) & 15u);
            solidWeight += weight;
            solidDensity += weight * minerals[static_cast<std::size_t>(index)].kgPerCubicMeter();
        }
        solidDensity /= std::max(solidWeight, 1.0f);
        const float actinides = float(geo::Mineral::nibble(passport.bulk, geo::Mineral::Kind::Actinides)) / 15.0f;
        const float metal = float(geo::Mineral::nibble(passport.bulk, geo::Mineral::Kind::Iron) + geo::Mineral::nibble(passport.bulk, geo::Mineral::Kind::Nickel)) / 30.0f;
        const float silicates = float(geo::Mineral::nibble(passport.bulk, geo::Mineral::Kind::Olivine) + geo::Mineral::nibble(passport.bulk, geo::Mineral::Kind::Pyroxene) + geo::Mineral::nibble(passport.bulk, geo::Mineral::Kind::Feldspar)) / 45.0f;
        const float escapeProxy = std::sqrt(std::max(2.0f * gravity * passport.radius, 0.0f));
        float greenhouse = 0.0f;
        float retainedWeight = 0.0f;
        geo::Volatile::Mix retained = 0;
        const auto& volatiles = geo::Volatile::table();
        for (integer index = 0; index < 8; ++index) {
            const auto kind = static_cast<geo::Volatile::Kind>(index);
            const integer amount = geo::Volatile::nibble(passport.volatiles, kind);
            const float molecular = glm::clamp(volatiles[static_cast<std::size_t>(index)].molarMass / 44.0f, 0.05f, 1.5f);
            const float thermalLoss = glm::clamp((equilibrium - 90.0f) / 360.0f, 0.0f, 1.0f);
            const float retention = glm::clamp(0.12f + escapeProxy / 850.0f + molecular * 0.34f - thermalLoss * (1.05f - molecular * 0.35f) - passport.ageGyr * 0.018f, 0.0f, 1.0f);
            const integer kept = static_cast<integer>(std::lround(float(amount) * retention));
            retained = geo::Volatile::pack(retained, kind, kept);
            retainedWeight += float(kept);
            greenhouse += float(kept) * volatiles[static_cast<std::size_t>(index)].greenhouse;
        }
        const float retainedBudget = glm::clamp(retainedWeight / 120.0f, 0.0f, 1.0f);
        greenhouse = retainedWeight > 0.0f ? greenhouse / retainedWeight : 0.0f;
        const float atmosphere = glm::clamp(retainedBudget * (0.55f + greenhouse * 0.85f) * (0.45f + gravity * 0.18f), 0.0f, 1.0f);
        const float temperature = equilibrium * (1.0f + atmosphere * greenhouse * 0.32f);
        const float liquidWindow = 1.0f - glm::smoothstep(315.0f, 390.0f, temperature);
        const float waterInventory = float(geo::Volatile::nibble(retained, geo::Volatile::Kind::Water)) / 15.0f;
        const float water = waterInventory * glm::smoothstep(185.0f, 273.0f, temperature) * liquidWindow;
        const float ice = waterInventory * (1.0f - glm::smoothstep(210.0f, 285.0f, temperature));
        const float primordialHeat = std::exp(-passport.ageGyr / 4.8f) * glm::clamp(std::log2(std::max(passport.radius, 1000.0f) / 1000.0f) / 13.0f, 0.08f, 1.0f);
        const float heat = glm::clamp(primordialHeat + actinides * 0.42f + passport.environment.tidalHeat * 0.65f, 0.0f, 1.0f);
        const float differentiation = glm::clamp(age * 0.72f + metal * 0.35f + silicates * 0.18f + primordialHeat * 0.22f, 0.0f, 1.0f);
        const float thickness = glm::clamp(0.82f - heat * 0.48f + age * 0.24f + solidDensity / 15000.0f, 0.12f, 0.96f);
        const float mobility = glm::clamp(heat * (0.18f + water * 0.72f) * (1.15f - thickness) + passport.environment.tidalHeat * 0.28f, 0.0f, 1.0f);
        const float fragmentation = glm::clamp(0.08f + mobility * 0.72f + heat * 0.22f + passport.environment.eccentricity * 0.18f, 0.0f, 1.0f);
        const integer plates = std::clamp(integer{1} + static_cast<integer>(std::lround(fragmentation * 20.0f)), integer{1}, integer{30});
        const float cohesion = glm::clamp(0.12f + thickness * 0.25f + differentiation * 0.18f - water * 0.12f, 0.05f, 0.95f);
        const float grain = glm::clamp(0.18f + age * 0.46f + passport.environment.debrisFlux * 0.34f, 0.0f, 1.0f);
        const float reliefFraction = glm::clamp(0.035f + cohesion * 0.025f + (1.0f - glm::clamp(gravity / 12.0f, 0.0f, 1.0f)) * 0.018f, 0.025f, 0.085f);
        const float reliefAmplitude = passport.radius * reliefFraction;
        return Geology{
            .crust = {.mix = passport.bulk, .plates = plates, .differentiation = differentiation, .thickness = thickness, .mobility = mobility, .fragmentation = fragmentation, .cohesion = cohesion, .grain = grain},
            .mantle = {.heat = heat, .plumeRate = glm::clamp(heat * (1.15f - mobility * 0.58f), 0.0f, 1.0f), .plumePower = glm::clamp(0.18f + heat * 0.72f + passport.environment.tidalHeat * 0.24f, 0.0f, 1.0f), .boundaryAffinity = glm::clamp(mobility * (0.42f + water * 0.45f), 0.0f, 1.0f)},
            .bombardment = {.mix = passport.bulk, .flux = glm::clamp(passport.environment.debrisFlux * (0.42f + age * 0.58f), 0.0f, 1.0f), .violence = glm::clamp(0.25f + passport.environment.debrisFlux * 0.52f + passport.environment.eccentricity * 0.30f, 0.0f, 1.0f), .largeBodyTail = glm::clamp(passport.environment.debrisFlux * 0.55f + passport.environment.eccentricity * 0.24f, 0.0f, 1.0f), .ironFraction = metal},
            .climate = {.retained = retained, .atmosphere = atmosphere, .temperature = temperature, .water = water, .ice = ice, .weathering = glm::clamp(water * atmosphere * age * 1.8f, 0.0f, 1.0f), .transport = glm::clamp(water * (0.35f + atmosphere) * (0.65f + mobility * 0.35f), 0.0f, 1.0f)},
            .history = {.surfaceAge = glm::clamp(age * (1.0f - mobility * 0.28f) + passport.environment.debrisFlux * 0.12f, 0.0f, 1.0f), .reliefAmplitude = reliefAmplitude},
        };
    }

    void Compose::form(Planet& planet, const Geology& geology) {
        const integer seed = planet.passport.seed;
        const float amplitude = geology.history.reliefAmplitude;
        Formation formation{planet.heights.pack, planet.farAlbedo.pack};
        vector<PlateSite> plates;
        plates.reserve(static_cast<std::size_t>(geology.crust.plates));
        for (integer index = 0; index < geology.crust.plates; ++index) {
            plates.push_back(PlateSite{
                .center = Sample::sphereDir(index, seed + 1009, 3, 5),
                .pole = Sample::sphereDir(index, seed + 1031, 7, 9),
                .speed = 0.45f + 0.55f * Sample::hash01(index, seed, 101, 37),
                .elevation = Sample::hash01(index, seed, 107, 41) * 2.0f - 1.0f,
                .age = glm::clamp(geology.history.surfaceAge * (0.72f + 0.40f * Sample::hash01(index, seed, 109, 43)), 0.0f, 1.0f),
                .felsic = glm::clamp(geology.crust.differentiation * (0.55f + 0.70f * Sample::hash01(index, seed, 113, 47)), 0.0f, 1.0f),
            });
        }
        PlateField::apply(formation, PlateField{.sites = plates, .seed = seed + 1009, .amplitude = amplitude, .width = 0.065f + 0.055f * geology.crust.fragmentation, .activity = glm::clamp(0.28f + geology.crust.fragmentation * 0.52f + geology.mantle.heat * 0.35f, 0.0f, 1.0f)});

        struct BoundaryCandidate {
            vec3 center;
            vec3 along;
            integer first;
            integer second;
            float divergence;
            float shear;
            float strength;
        };
        vector<BoundaryCandidate> candidates;
        candidates.reserve(192);
        for (integer candidate = 0; candidate < 192; ++candidate) {
            const vec3 direction = Sample::sphereDir(candidate, seed + 1103, 11, 13);
            const PlateSite::Hit hit = PlateSite::hit(direction, plates);
            const float fieldBoundary = formation.boundary.at(direction);
            const float fieldFracture = formation.fracture.at(direction);
            if (plates.size() <= 1 or (std::abs(fieldBoundary) < 0.025f and fieldFracture < 0.08f))
                continue;
            const vec3 normalRaw = plates[static_cast<std::size_t>(hit.first)].center - plates[static_cast<std::size_t>(hit.second)].center;
            const vec3 projected = normalRaw - direction * glm::dot(normalRaw, direction);
            if (glm::dot(projected, projected) < 1.0e-8f)
                continue;
            const vec3 normal = glm::normalize(projected);
            const vec3 along = glm::normalize(glm::cross(direction, normal));
            candidates.push_back(BoundaryCandidate{.center = direction, .along = along, .first = hit.first, .second = hit.second, .divergence = fieldBoundary, .shear = fieldFracture, .strength = std::abs(fieldBoundary) + fieldFracture * 0.65f});
        }
        std::sort(candidates.begin(), candidates.end(), [](const BoundaryCandidate& a, const BoundaryCandidate& b) { return a.strength + (a.divergence > 0.0f ? 1.0f : 0.0f) > b.strength + (b.divergence > 0.0f ? 1.0f : 0.0f); });
        vector<vec3> usedBoundaries;
        const integer boundaryEvents = std::clamp(integer{1} + static_cast<integer>(std::lround(geology.crust.fragmentation * 4.0f + geology.mantle.plumeRate * 2.0f)), integer{1}, integer{6});
        for (const BoundaryCandidate& candidate : candidates) {
            if (static_cast<integer>(usedBoundaries.size()) >= boundaryEvents)
                break;
            bool separated = true;
            for (vec3 used : usedBoundaries) {
                if (glm::dot(used, candidate.center) > std::cos(0.28f)) {
                    separated = false;
                    break;
                }
            }
            if (not separated)
                continue;
            usedBoundaries.push_back(candidate.center);
            const float scale = glm::clamp(std::abs(candidate.divergence) * 0.75f + candidate.shear * 0.45f + geology.crust.fragmentation * 0.35f, 0.18f, 1.0f);
            Rift::apply(formation.relief, Rift{.center = candidate.center, .along = candidate.along, .halfWidth = 0.012f + 0.030f * scale, .halfLength = 0.24f + 0.34f * scale, .depth = amplitude * (0.12f + 0.24f * scale), .seed = seed + 1201 + static_cast<integer>(usedBoundaries.size()) * 31});
        }

        const integer basinCount = std::clamp(static_cast<integer>(std::lround(geology.bombardment.largeBodyTail * 5.0f)), integer{0}, integer{6});
        for (integer impact = 0; impact < basinCount; ++impact) {
            const vec3 axis = Sample::sphereDir(impact, seed + 2003, 17, 19);
            const float radius = 0.10f + 0.16f * Sample::hash01(impact, seed, 2011, 41) * geology.bombardment.violence;
            const float depth = amplitude * (0.18f + 0.34f * geology.bombardment.violence) * (0.65f + 0.35f * Sample::hash01(impact, seed, 2017, 43));
            Basin::apply(formation, Basin{.center = axis, .along = Sample::sphereDir(impact, seed + 2019, 23, 29), .radius = radius, .depth = depth, .obliquity = 0.25f + 0.65f * Sample::hash01(impact, seed, 2021, 47), .exogenic = geology.bombardment.ironFraction, .seed = seed + 2027 + impact * 47});
        }
        Burst::apply(formation.relief, Burst::epoch(Burst::Epoch{.seed = seed, .salt = 211, .count = 28 + static_cast<integer>(90.0f * geology.bombardment.flux), .radiusMin = 0.018f, .radiusSpan = 0.075f, .depthMin = amplitude * 0.04f, .depthSpan = amplitude * (0.08f + 0.08f * geology.bombardment.violence), .highland = 1.0f, .avoidAxis = vec3{0.0f, 1.0f, 0.0f}, .avoidDot = 2.0f}));
        Erode::apply(formation.relief, Erode{.years = geology.history.surfaceAge, .strength = 0.18f + geology.climate.weathering * 0.48f, .north = 0.0f, .iterations = 2 + static_cast<integer>(std::lround(geology.climate.weathering * 4.0f)), .seed = seed + 2203});

        const integer plumeCount = geology.mantle.plumeRate > 0.045f ? std::clamp(integer{1} + static_cast<integer>(std::lround(geology.mantle.plumeRate * 5.0f)), integer{1}, integer{8}) : integer{0};
        for (integer plume = 0; plume < plumeCount; ++plume) {
            vec3 axis = Sample::sphereDir(plume, seed + 3001, 23, 29);
            if (geology.mantle.boundaryAffinity > Sample::hash01(plume, seed, 3007, 47) and not candidates.empty())
                axis = candidates[static_cast<std::size_t>(plume % static_cast<integer>(candidates.size()))].center;
            const float residence = 1.0f - geology.crust.mobility;
            const float power = glm::clamp(geology.mantle.plumePower * (0.85f + residence * 1.65f) * (0.72f + 0.52f * Sample::hash01(plume, seed, 3011, 53)), 0.18f, 1.25f);
            const float radius = 0.09f + 0.11f * power;
            Swell::apply(formation.relief, Swell{.axis = axis, .sigma = 0.24f + 0.26f * power, .amplitude = amplitude * (0.14f + 0.28f * power), .seed = seed + 3023 + plume * 59});
            vec3 tangent = glm::cross(axis, Sample::sphereDir(plume, seed + 3037, 31, 37));
            if (glm::dot(tangent, tangent) < 1.0e-8f)
                tangent = glm::cross(axis, vec3{0.0f, 1.0f, 0.0f});
            tangent = glm::normalize(tangent);
            const integer chain = 1 + static_cast<integer>(std::lround(geology.crust.mobility * 3.0f));
            for (integer volcano = 0; volcano < chain; ++volcano) {
                const float travel = geology.crust.mobility * 0.06f * float(volcano);
                const vec3 vent = glm::normalize(axis + tangent * travel);
                Burst::apply(formation.relief, Burst::eruption(vent, radius * (1.0f - 0.11f * float(volcano)), amplitude * (0.28f + 0.48f * power) / (1.0f + 0.24f * float(volcano)), seed + 3109 + plume * 101 + volcano * 17));
                Volcanic::stamp(formation.volcanic, vent, radius * 1.15f, power, seed + 3203 + plume * 101 + volcano * 17);
            }
        }

        if (geology.climate.transport > 0.025f)
            Drainage::apply(formation.relief, Drainage{.seed = seed + 4001, .sources = 4 + static_cast<integer>(std::lround(24.0f * geology.climate.transport)), .steps = 45 + static_cast<integer>(std::lround(95.0f * geology.climate.transport)), .stepLength = 0.0028f + 0.0024f * geology.climate.transport, .width = 0.0012f + 0.0014f * geology.climate.transport, .depth = amplitude * (0.006f + 0.026f * geology.climate.transport)});
        Bombardment::apply(formation.relief, Bombardment{.seed = seed + 5003, .count = 350 + static_cast<integer>(std::lround(2400.0f * geology.bombardment.flux)), .radiusMin = 0.0015f, .radiusMax = 0.006f + 0.008f * geology.bombardment.violence, .depth = amplitude * (0.006f + 0.018f * geology.bombardment.violence), .northDensity = 1.0f});
        Erode::apply(formation.relief, Erode{.years = geology.history.surfaceAge, .strength = 0.08f + geology.climate.weathering * 0.32f, .north = 0.0f, .iterations = 1 + static_cast<integer>(std::lround(geology.climate.weathering * 3.0f)), .seed = seed + 5101});

        for (integer index = 0; index < formation.water.pack.storedCount(); ++index) {
            const auto slot = formation.water.pack.slotOf(index);
            const vec3 direction = formation.water.pack.direction(slot);
            const float relief = formation.relief.at(direction) / std::max(amplitude, 1.0f);
            const float latitudeIce = glm::smoothstep(0.52f, 0.88f, std::abs(direction.y));
            formation.water.at(slot) = glm::clamp(geology.climate.water * (0.72f - relief * 0.42f) + geology.climate.ice * latitudeIce, 0.0f, 1.0f);
            formation.sediment.at(slot) = glm::clamp(geology.climate.transport * formation.water.at(slot) * (0.55f + formation.fracture.at(slot) * 0.35f), 0.0f, 1.0f);
        }
        planet.runtime.surfaceAcceleration = float(6.67430e-11 * std::max(planet.passport.mass, 0.0) / (double(planet.passport.radius) * double(planet.passport.radius)));
        planet.runtime.reliefAmplitude = amplitude;
        planet.runtime.atmosphere.seaDensity = geology.climate.atmosphere * 1800.0f;
        planet.runtime.atmosphere.kerman = planet.passport.radius * glm::clamp(0.008f + geology.climate.temperature / std::max(planet.runtime.surfaceAcceleration, 0.2f) * 0.00012f, 0.008f, 0.055f);
        planet.runtime.atmosphere.outerRadius = planet.passport.radius + planet.runtime.atmosphere.kerman * 3.0f;
        const AtmosphereLook look = AtmosphereLook::of(geology);
        planet.runtime.atmosphere.day = look.day;
        planet.runtime.atmosphere.zenithTau = look.zenithTau;
        for (integer index = 0; index < planet.heights.pack.storedCount(); ++index) {
            const auto slot = planet.heights.pack.slotOf(index);
            planet.heights.at(slot) = planet.encodeRelief(glm::clamp(formation.relief.at(slot), -amplitude, amplitude));
        }
        planet.heights.stitch();
        Compose::paint(planet, formation, geology);
    }

    void Generator::mars(Planet& planet) {
        const Geology geology = Compose::derive(planet.passport);
        const float amplitude = geology.history.reliefAmplitude;
        const float grain = glm::clamp(geology.crust.grain, 0.0f, 1.0f);
        const float tectonic = glm::clamp(geology.mantle.heat, 0.0f, 1.0f);
        const float differentiation = glm::clamp(geology.crust.differentiation, 0.0f, 1.0f);
        const float age = glm::clamp(geology.history.surfaceAge, 0.0f, 1.0f);
        const integer seed = planet.passport.seed;
        const vec3 tharsis = glm::normalize(vec3{0.72f, 0.12f, 0.35f});
        const vec3 olympus = glm::normalize(tharsis + vec3{0.04f, 0.08f, -0.02f});
        const vec3 arsia = glm::normalize(tharsis + vec3{-0.12f, -0.08f, 0.10f});
        const vec3 pavonis = glm::normalize(tharsis + vec3{-0.04f, -0.02f, 0.08f});
        const vec3 ascrea = glm::normalize(tharsis + vec3{0.05f, 0.04f, 0.12f});
        const vec3 canyonCenter = glm::normalize(tharsis + vec3{0.35f, -0.08f, -0.22f});
        const vec3 canyonAlong = glm::normalize(glm::cross(vec3{0.0f, 1.0f, 0.0f}, canyonCenter));
        const vec3 canyonAcross = glm::normalize(glm::cross(canyonCenter, canyonAlong));
        const vec3 hellas = glm::normalize(vec3{0.18f, -0.72f, 0.52f});
        const vec3 argyre = glm::normalize(vec3{-0.48f, -0.68f, 0.22f});
        const vec3 isidis = glm::normalize(vec3{0.58f, 0.04f, -0.52f});
        const float worn = 0.55f + 0.45f * age;
        geo::IcosaMap<float> relief{planet.heights.pack, 0.0f};

        Provinces::apply(relief, Provinces{.count = 2, .seed = seed, .amplitude = amplitude * (0.18f + 0.12f * differentiation)});

        Burst::apply(relief, vector<Burst>{
            Burst::impact(hellas, 0.24f, amplitude * 0.48f * worn, seed + 101),
            Burst::impact(argyre, 0.14f, amplitude * 0.29f * worn, seed + 137),
            Burst::impact(isidis, 0.11f, amplitude * 0.20f * worn, seed + 173),
        });
        Burst::apply(relief, Burst::epoch(Burst::Epoch{.seed = seed, .salt = 3, .count = 42 + integer(56.0f * grain), .radiusMin = 0.025f, .radiusSpan = 0.075f, .depthMin = amplitude * 0.07f * worn, .depthSpan = amplitude * 0.13f * worn, .highland = 0.16f, .avoidAxis = olympus, .avoidDot = 0.97f}));
        Erode::apply(relief, Erode{.years = age, .strength = 0.58f, .north = 1.0f, .iterations = 4, .seed = seed + 251});

        Swell::apply(relief, Swell{.axis = tharsis, .sigma = 0.44f, .amplitude = amplitude * (0.26f + 0.16f * tectonic), .seed = seed + 307});
        Burst::apply(relief, vector<Burst>{
            Burst::eruption(olympus, 0.16f, amplitude * (0.36f + 0.16f * tectonic), seed + 331),
            Burst::eruption(arsia, 0.10f, amplitude * 0.18f, seed + 347),
            Burst::eruption(pavonis, 0.09f, amplitude * 0.15f, seed + 367),
            Burst::eruption(ascrea, 0.095f, amplitude * 0.17f, seed + 389),
        });
        Rift::apply(relief, Rift{.center = canyonCenter, .along = canyonAlong, .halfWidth = 0.04f + 0.02f * differentiation, .halfLength = 0.46f, .depth = amplitude * (0.24f + 0.22f * differentiation), .seed = seed + 401});
        const vector<Rift> minorRifts{
            Rift{.center = glm::normalize(canyonCenter - canyonAlong * 0.25f + canyonAcross * 0.08f), .along = glm::normalize(canyonAlong + canyonAcross * 0.26f), .halfWidth = 0.014f, .halfLength = 0.17f, .depth = amplitude * 0.095f, .seed = seed + 431},
            Rift{.center = glm::normalize(canyonCenter + canyonAlong * 0.22f - canyonAcross * 0.07f), .along = glm::normalize(canyonAlong - canyonAcross * 0.31f), .halfWidth = 0.011f, .halfLength = 0.14f, .depth = amplitude * 0.075f, .seed = seed + 439},
            Rift{.center = glm::normalize(canyonCenter + canyonAcross * 0.13f), .along = glm::normalize(canyonAlong + canyonAcross * 0.12f), .halfWidth = 0.009f, .halfLength = 0.11f, .depth = amplitude * 0.062f, .seed = seed + 443},
            Rift{.center = glm::normalize(canyonCenter - canyonAcross * 0.15f - canyonAlong * 0.06f), .along = glm::normalize(canyonAlong - canyonAcross * 0.18f), .halfWidth = 0.008f, .halfLength = 0.09f, .depth = amplitude * 0.052f, .seed = seed + 449},
        };
        for (const Rift& rift : minorRifts)
            Rift::apply(relief, rift);
        Drainage::apply(relief, Drainage{.seed = seed + 601, .sources = 10 + integer(12.0f * grain), .steps = 90, .stepLength = 0.0035f, .width = 0.0015f + 0.0005f * grain, .depth = amplitude * (0.014f + 0.008f * age)});

        Burst::apply(relief, Burst::epoch(Burst::Epoch{.seed = seed, .salt = 41, .count = 20 + integer(28.0f * grain), .radiusMin = 0.014f, .radiusSpan = 0.045f, .depthMin = amplitude * 0.035f, .depthSpan = amplitude * 0.075f, .highland = 1.0f, .avoidAxis = olympus, .avoidDot = 0.92f}));
        Bombardment::apply(relief, Bombardment{.seed = seed + 701, .count = 1200 + integer(1000.0f * grain * age), .radiusMin = 0.0018f, .radiusMax = 0.010f, .depth = amplitude * (0.012f + 0.008f * grain), .northDensity = 0.34f + 0.18f * (1.0f - age)});
        Erode::apply(relief, Erode{.years = 0.32f + 0.28f * age, .strength = 0.28f, .north = 0.35f, .iterations = 3, .seed = seed + 457});

        const integer count = planet.heights.pack.storedCount();
        for (integer index = 0; index < count; ++index) {
            const auto slot = planet.heights.pack.slotOf(index);
            planet.heights.at(slot) = planet.encodeRelief(glm::clamp(relief.at(slot), -amplitude, amplitude));
        }
        planet.heights.stitch();
        PaintCover::apply(planet, PaintCover::mars(geology, planet.passport.seed));
    }

}
