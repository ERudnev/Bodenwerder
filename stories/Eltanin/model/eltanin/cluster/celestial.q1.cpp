#include <eltanin/cluster/celestial.q1.h>

#include <algorithm>
#include <cmath>
#include <format>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::cluster {

    using namespace fqsm::api;
    using chemistry::Volatile;

    namespace {

        auto nibble(Volatile::Mix mix, Volatile::Kind kind) -> integer {
            return Volatile::nibble(mix, kind);
        }

        auto wolfRayet(Kelvins temperature, Volatile::Mix mix) -> bool {
            const integer hydrogen = nibble(mix, Volatile::Kind::Hydrogen);
            const integer helium = nibble(mix, Volatile::Kind::Helium);
            const integer carbon = nibble(mix, Volatile::Kind::Carbon);
            const integer nitrogen = nibble(mix, Volatile::Kind::Nitrogen);
            const integer oxygen = nibble(mix, Volatile::Kind::Oxygen);
            return temperature >= 20000.0f and hydrogen <= 2 and (helium + carbon + nitrogen + oxygen) >= 8;
        }

        auto carbonStar(Kelvins temperature, Volatile::Mix mix) -> bool {
            return temperature < 4500.0f and nibble(mix, Volatile::Kind::Carbon) > nibble(mix, Volatile::Kind::Oxygen) and nibble(mix, Volatile::Kind::Carbon) >= 3;
        }

        auto wrClass(Volatile::Mix mix) -> string {
            const integer carbon = nibble(mix, Volatile::Kind::Carbon);
            const integer nitrogen = nibble(mix, Volatile::Kind::Nitrogen);
            const integer oxygen = nibble(mix, Volatile::Kind::Oxygen);
            if (oxygen >= carbon and oxygen >= nitrogen and oxygen >= 3)
                return "WO";
            if (carbon >= nitrogen and carbon >= 3)
                return "WC";
            if (nitrogen >= 3)
                return "WN";
            return "WR";
        }

        auto photosphereRgb(Kelvins kelvin) -> vec3 {
            struct Knot { float temperature; vec3 rgb; };
            constexpr Knot knots[] = {
                {2000.0f, {1.00f, 0.16f, 0.00f}},
                {2400.0f, {1.00f, 0.30f, 0.03f}},
                {3700.0f, {1.00f, 0.50f, 0.12f}},
                {4800.0f, {1.00f, 0.66f, 0.28f}},
                {5772.0f, {1.00f, 0.80f, 0.42f}},
                {6500.0f, {1.00f, 0.90f, 0.72f}},
                {7500.0f, {0.86f, 0.90f, 1.00f}},
                {10000.0f, {0.58f, 0.74f, 1.00f}},
                {15000.0f, {0.44f, 0.62f, 1.00f}},
                {30000.0f, {0.32f, 0.50f, 1.00f}},
                {40000.0f, {0.26f, 0.44f, 1.00f}},
            };
            const std::size_t last = sizeof(knots) / sizeof(knots[0]) - 1;
            const float temperature = std::clamp(float(kelvin), knots[0].temperature, knots[last].temperature);
            for (std::size_t index = 1; index <= last; ++index) {
                if (temperature <= knots[index].temperature) {
                    const float span = std::max(knots[index].temperature - knots[index - 1].temperature, 1.0f);
                    return glm::mix(knots[index - 1].rgb, knots[index].rgb, (temperature - knots[index - 1].temperature) / span);
                }
            }
            return knots[last].rgb;
        }

    }

    auto Star::Quantum::look() const -> vec3 {
        vec3 photosphere = photosphereRgb(temperature);
        if (wolfRayet(temperature, mix))
            photosphere = glm::mix(photosphere, vec3{0.55f, 0.78f, 1.00f}, 0.35f);
        else if (carbonStar(temperature, mix))
            photosphere = glm::mix(photosphere, vec3{1.00f, 0.28f, 0.06f}, 0.40f);
        const float luma = glm::dot(photosphere, vec3{0.2126f, 0.7152f, 0.0722f});
        return glm::clamp(glm::mix(vec3{luma, luma, luma}, photosphere, 1.35f), vec3{0.0f, 0.0f, 0.0f}, vec3{1.0f, 1.0f, 1.0f});
    }

    auto Star::Quantum::spectralClass() const -> string {
        if (wolfRayet(temperature, mix))
            return wrClass(mix);
        if (carbonStar(temperature, mix))
            return "C";
        struct Band { char letter; float hot; float cold; };
        constexpr Band bands[] = {
            {'O', 50000.0f, 30000.0f},
            {'B', 30000.0f, 10000.0f},
            {'A', 10000.0f, 7500.0f},
            {'F', 7500.0f, 6000.0f},
            {'G', 6000.0f, 5200.0f},
            {'K', 5200.0f, 3700.0f},
            {'M', 3700.0f, 2400.0f},
        };
        for (const auto& band : bands) {
            if (temperature >= band.cold) {
                const float span = std::max(band.hot - band.cold, 1.0f);
                const integer subtype = std::clamp(integer(std::clamp((band.hot - temperature) / span, 0.0f, 1.0f) * 10.0f), integer{0}, integer{9});
                return std::format("{}{} V", band.letter, subtype);
            }
        }
        return "L";
    }

    auto Star::Quantum::kind() const -> string {
        if (wolfRayet(temperature, mix))
            return "Wolf-Rayet";
        if (carbonStar(temperature, mix))
            return "Carbon star";
        if (temperature >= 30000.0f)
            return "Blue star";
        if (temperature >= 10000.0f)
            return "Blue-white star";
        if (temperature >= 7500.0f)
            return "White star";
        if (temperature >= 6000.0f)
            return "Yellow-white dwarf";
        if (temperature >= 5200.0f)
            return "Yellow dwarf";
        if (temperature >= 3700.0f)
            return "Orange dwarf";
        if (temperature >= 2400.0f)
            return "Red dwarf";
        return "Brown dwarf";
    }

    auto Star::Quantum::metallicity() const -> string {
        const integer hydrogen = nibble(mix, Volatile::Kind::Hydrogen);
        const integer iron = nibble(mix, Volatile::Kind::Iron);
        if (hydrogen <= 0 or iron <= 0)
            return "—";
        constexpr double solarFeH = 1.0 / 15.0;
        const double feH = (double(iron) / double(hydrogen)) / solarFeH;
        return std::format("[Fe/H] {:+.2f}", std::log10(feH));
    }

    auto doctrine::celestial() -> Schema {
        return ask::schema::merge({
            ask::schema::aspect<Celestial>(),
            ask::schema::aspect<Star>(),
        });
    }

}
