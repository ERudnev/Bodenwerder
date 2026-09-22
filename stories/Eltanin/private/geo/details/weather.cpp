#include "geo/details/weather.h"
#include "geo/celestial/planet.h"
#include "geo/details/compose.h"
#include "geo/details/effects.h"

#include <eltanin/geo/minerals.q1.h>
#include <eltanin/geo/volatiles.q1.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto nibble(geo::Mineral::Mix mix, geo::Mineral::Kind kind) -> float {
            return float(geo::Mineral::nibble(mix, kind)) / 15.0f;
        }

        auto nibble(geo::Volatile::Mix mix, geo::Volatile::Kind kind) -> float {
            return float(geo::Volatile::nibble(mix, kind)) / 15.0f;
        }

        auto encodeWind(float value) -> std::uint8_t {
            return static_cast<std::uint8_t>(std::lround(glm::clamp(value * 0.5f / 40.0f + 0.5f, 0.0f, 1.0f) * 255.0f));
        }

        auto encodeUnit(float value) -> std::uint8_t {
            return static_cast<std::uint8_t>(std::lround(glm::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        auto clampSlot(const geo::IcosaPack& pack, integer diamond, integer iu, integer iv) -> geo::IcosaPack::Slot {
            const integer last = pack.edgeSegments();
            return geo::IcosaPack::Slot{.diamond = diamond, .iu = std::clamp(iu, integer{0}, last), .iv = std::clamp(iv, integer{0}, last)};
        }

    }

    auto Weather::decksOf(const Geology& geology, float kerman) -> vector<Deck> {
        if (geology.climate.atmosphere <= 0.0f or kerman <= 0.0f)
            return {};
        vector<Deck> decks;
        const float oxides = nibble(geology.crust.mix, geo::Mineral::Kind::Oxides);
        const float clay = nibble(geology.crust.mix, geo::Mineral::Kind::Clay);
        const float dustPotential = (oxides * 0.65f + clay * 0.35f) * (1.0f - geology.crust.cohesion) * (1.0f - geology.climate.water);
        if (dustPotential > 0.12f) {
            const auto& oxide = geo::Mineral::table()[static_cast<std::size_t>(geo::Mineral::Kind::Oxides)];
            decks.push_back(Deck{.kind = Kind::Dust, .base = 0.0f, .top = kerman * (0.10f + 0.12f * dustPotential), .scatter = RGB{oxide.albedo.x, oxide.albedo.y, oxide.albedo.z}, .channel = 0.0f, .actor = {}});
        }
        const float waterInv = nibble(geology.climate.retained, geo::Volatile::Kind::Water);
        const float carbonDioxide = nibble(geology.climate.retained, geo::Volatile::Kind::CarbonDioxide);
        const float methane = nibble(geology.climate.retained, geo::Volatile::Kind::Methane);
        const float temperature = geology.climate.temperature;
        const bool waterCloud = waterInv > 0.08f and temperature > 210.0f and temperature < 330.0f and geology.climate.atmosphere > 0.08f;
        const bool carbonFrost = carbonDioxide > 0.18f and temperature < 205.0f;
        const bool methaneCloud = methane > 0.12f and temperature < 125.0f;
        if (waterCloud or carbonFrost or methaneCloud) {
            const vec3 tint = waterCloud ? vec3{0.92f, 0.94f, 0.98f} : carbonFrost ? vec3{0.82f, 0.88f, 0.95f} : vec3{0.88f, 0.78f, 0.62f};
            decks.push_back(Deck{.kind = Kind::Condensate, .base = kerman * 0.18f, .top = kerman * 0.55f, .scatter = RGB{tint.x, tint.y, tint.z}, .channel = 1.0f, .actor = {}});
        }
        const float ammonia = nibble(geology.climate.retained, geo::Volatile::Kind::Ammonia);
        const float sulfur = nibble(geology.climate.retained, geo::Volatile::Kind::SulfurDioxide);
        const float hazeMass = methane + ammonia + sulfur;
        if (hazeMass > 0.18f) {
            const vec3 tint = glm::normalize(vec3{0.82f, 0.62f, 0.28f} * methane + vec3{0.88f, 0.84f, 0.70f} * ammonia + vec3{0.92f, 0.78f, 0.36f} * sulfur) * 0.9f;
            decks.push_back(Deck{.kind = Kind::Haze, .base = kerman * 0.60f, .top = kerman * 1.20f, .scatter = RGB{tint.x, tint.y, tint.z}, .channel = 1.0f, .actor = {}});
        }
        if (static_cast<integer>(decks.size()) > 3)
            decks.resize(3);
        return decks;
    }

    auto Weather::spawn(const Passport& passport, const Geology& geology, integer heightSegments, float kerman, float seaDensity) -> base::maybe<Weather> {
        if (seaDensity <= 0.0f)
            return {};
        const integer segments = std::clamp(heightSegments / 16, integer{48}, integer{64});
        const geo::IcosaPack pack{.edgeBase = segments, .tessellation = 0};
        const AtmosphereLook look = AtmosphereLook::of(geology);
        const vec3 oxide = geo::Mineral::table()[static_cast<std::size_t>(geo::Mineral::Kind::Oxides)].albedo;
        Weather weather{
            .crust = geology.crust.mix,
            .retained = geology.climate.retained,
            .cohesion = geology.crust.cohesion,
            .water = geology.climate.water,
            .ice = geology.climate.ice,
            .column = geology.climate.atmosphere,
            .temperature = geology.climate.temperature,
            .decks = decksOf(geology, kerman),
            .climate = {pack, geology.climate.temperature},
            .heat = {pack, geology.climate.temperature},
            .wind = {pack, vec2{0.0f, 0.0f}},
            .cloud = {pack, 0.0f},
            .dust = {pack, 0.0f},
            .debt = 0,
            .baseDay = look.day,
            .baseTau = look.zenithTau,
            .dustTint = oxide,
            .atlas = {},
        };
        const float dustSeed = glm::clamp((nibble(geology.crust.mix, geo::Mineral::Kind::Oxides) * 0.65f + nibble(geology.crust.mix, geo::Mineral::Kind::Clay) * 0.35f) * (1.0f - geology.crust.cohesion) * (1.0f - geology.climate.water), 0.0f, 1.0f);
        for (integer index = 0; index < pack.storedCount(); ++index) {
            const auto slot = pack.slotOf(index);
            const vec3 direction = pack.direction(slot);
            const float baseline = ClimateField::temperature(passport, geology, direction);
            weather.climate.at(slot) = baseline;
            weather.heat.at(slot) = baseline;
            const float banks = glm::smoothstep(0.20f, 0.58f, Sample::fractal(direction * 2.8f, 29, 3, 0.48f));
            const float storm = glm::smoothstep(0.28f, 0.68f, Sample::fractal(direction * 2.2f, 17, 3, 0.52f));
            weather.cloud.at(slot) = glm::clamp(geology.climate.water * banks * 0.70f, 0.0f, 1.0f);
            weather.dust.at(slot) = glm::clamp(dustSeed * storm * 0.45f, 0.0f, 1.0f);
        }
        weather.climate.stitch();
        weather.heat.stitch();
        weather.cloud.stitch();
        weather.dust.stitch();
        return weather;
    }

    void Weather::tick(vec3 sunLocal, float stellarFlux, seconds dt) {
        const float step = std::max(float(dt), 1.0e-3f);
        const vec3 sun = glm::length(sunLocal) > 1.0e-6f ? glm::normalize(sunLocal) : vec3{0.0f, 1.0f, 0.0f};
        const float insolationScale = glm::clamp(stellarFlux / 1361.0f, 0.05f, 2.4f);
        const float tau = glm::mix(220.0f, 6400.0f, glm::clamp(cohesion, 0.0f, 1.0f));
        const float blend = 1.0f - std::exp(-step / tau);
        const integer count = heat.pack.storedCount();
        const auto& pack = heat.pack;
        for (integer index = 0; index < count; ++index) {
            const auto slot = pack.slotOf(index);
            const vec3 direction = pack.direction(slot);
            const float baseline = climate.at(slot);
            const float shade = glm::clamp(1.0f - cloud.at(slot) - dust.at(slot) * 0.70f, 0.12f, 1.0f);
            const float insol = glm::clamp(glm::dot(direction, sun), 0.0f, 1.0f) * shade * insolationScale;
            const float target = glm::mix(baseline * 0.62f, baseline * (0.82f + 0.55f * insol), glm::smoothstep(0.0f, 0.18f, insol));
            heat.at(slot) = glm::mix(heat.at(slot), target, blend);
        }
        heat.stitch();
        const float thermalGain = 0.18f;
        for (integer index = 0; index < count; ++index) {
            const auto slot = pack.slotOf(index);
            const vec3 direction = pack.direction(slot);
            const float dU = heat.at(clampSlot(pack, slot.diamond, slot.iu + 1, slot.iv)) - heat.at(clampSlot(pack, slot.diamond, slot.iu - 1, slot.iv));
            const float dV = heat.at(clampSlot(pack, slot.diamond, slot.iu, slot.iv + 1)) - heat.at(clampSlot(pack, slot.diamond, slot.iu, slot.iv - 1));
            const float dusk = 1.0f - glm::smoothstep(0.08f, 0.42f, std::abs(glm::dot(direction, sun)));
            const vec2 thermal{-dU * thermalGain, -dV * thermalGain};
            const vec2 hadley{0.0f, -direction.y * 9.0f};
            const vec2 terminator{dusk * 7.0f, 0.0f};
            const float rumble = Sample::noise(direction * 3.5f, 41) * 1.4f;
            vec2 flow = thermal + hadley + terminator + vec2{rumble, Sample::noise(direction * 2.8f, 67) * 1.1f};
            const float speed = glm::length(flow);
            if (speed > 36.0f)
                flow *= 36.0f / speed;
            wind.at(slot) = flow;
        }
        wind.stitch();
        const float moisture = glm::clamp(water + ice * 0.55f, 0.0f, 1.0f);
        const float dry = 1.0f - glm::clamp(water, 0.0f, 1.0f);
        const float lift = 16.0f + cohesion * 14.0f;
        vector<float> nextCloud(static_cast<std::size_t>(count), 0.0f);
        vector<float> nextDust(static_cast<std::size_t>(count), 0.0f);
        for (integer index = 0; index < count; ++index) {
            const auto slot = pack.slotOf(index);
            const vec2 flow = wind.at(slot);
            const float dU = wind.at(clampSlot(pack, slot.diamond, slot.iu + 1, slot.iv)).x - wind.at(clampSlot(pack, slot.diamond, slot.iu - 1, slot.iv)).x;
            const float dV = wind.at(clampSlot(pack, slot.diamond, slot.iu, slot.iv + 1)).y - wind.at(clampSlot(pack, slot.diamond, slot.iu, slot.iv - 1)).y;
            const float converge = glm::clamp(-(dU + dV) * 0.08f, -0.35f, 0.85f);
            const integer du = flow.x > 2.0f ? 1 : flow.x < -2.0f ? -1 : 0;
            const integer dv = flow.y > 2.0f ? -1 : flow.y < -2.0f ? 1 : 0;
            const auto back = clampSlot(pack, slot.diamond, slot.iu - du, slot.iv - dv);
            float covered = cloud.at(back);
            covered += converge * moisture * step * 0.25f;
            covered *= std::exp(-step / 90.0f);
            nextCloud[static_cast<std::size_t>(index)] = glm::clamp(covered, 0.0f, 1.0f);
            float airborne = dust.at(back);
            const float gust = glm::length(flow);
            if (gust > lift)
                airborne += (gust - lift) * dry * (1.0f - cohesion) * step * 0.006f;
            airborne *= std::exp(-step / 36.0f);
            nextDust[static_cast<std::size_t>(index)] = glm::clamp(airborne, 0.0f, 1.0f);
        }
        for (integer index = 0; index < count; ++index) {
            const auto slot = cloud.pack.slotOf(index);
            cloud.at(slot) = nextCloud[static_cast<std::size_t>(index)];
            dust.at(slot) = nextDust[static_cast<std::size_t>(index)];
        }
        cloud.stitch();
        dust.stitch();
    }

    auto Weather::atlasPixels() const -> vector<std::uint8_t> {
        const auto& pack = heat.pack;
        const integer span = pack.edgeVertices();
        const integer layers = geo::IcosaPack::diamondCount;
        vector<std::uint8_t> packed(static_cast<std::size_t>(layers) * static_cast<std::size_t>(span) * static_cast<std::size_t>(span) * 4u, std::uint8_t{0});
        for (integer index = 0; index < pack.storedCount(); ++index) {
            const auto slot = pack.slotOf(index);
            const vec2 flow = wind.at(slot);
            const std::size_t offset = static_cast<std::size_t>((slot.diamond * span + slot.iv) * span + slot.iu) * 4u;
            packed[offset] = encodeWind(flow.x);
            packed[offset + 1] = encodeWind(flow.y);
            packed[offset + 2] = encodeUnit(cloud.at(slot));
            packed[offset + 3] = encodeUnit(dust.at(slot));
        }
        return packed;
    }

    auto Weather::meanDust() const -> float {
        float sum = 0.0f;
        const integer count = dust.pack.storedCount();
        for (integer index = 0; index < count; ++index)
            sum += dust.at(dust.pack.slotOf(index));
        return sum / float(std::max(count, integer{1}));
    }

    auto Weather::meanCloud() const -> float {
        float sum = 0.0f;
        const integer count = cloud.pack.storedCount();
        for (integer index = 0; index < count; ++index)
            sum += cloud.at(cloud.pack.slotOf(index));
        return sum / float(std::max(count, integer{1}));
    }

    auto Weather::meanWind() const -> float {
        float sum = 0.0f;
        const integer count = wind.pack.storedCount();
        for (integer index = 0; index < count; ++index)
            sum += glm::length(wind.at(wind.pack.slotOf(index)));
        return sum / float(std::max(count, integer{1}));
    }

    auto Weather::windAt(vec3 direction) const -> vec2 {
        if (glm::length(direction) < 1.0e-6f)
            return vec2{0.0f, 0.0f};
        return wind.at(glm::normalize(direction));
    }

    void Weather::applyLook(Planet& planet) const {
        const float airborne = glm::clamp(meanDust(), 0.0f, 1.0f);
        const vec3 day = glm::mix(vec3{baseDay.x, baseDay.y, baseDay.z}, dustTint, airborne * 0.42f);
        planet.runtime.atmosphere.day = RGB{day.x, day.y, day.z};
        planet.runtime.atmosphere.zenithTau = glm::clamp(baseTau + airborne * 0.32f, 0.0f, 1.25f);
    }

}
