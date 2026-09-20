#include <eltanin/geo/volatiles.q1.h>

#include <algorithm>

namespace eltanin::geo {

    auto Volatile::pack(Mix mix, Kind kind, integer value) -> Mix {
        const integer shift = static_cast<integer>(kind) * 4;
        const Mix mask = Mix{15u} << shift;
        return (mix & ~mask) | (Mix(std::clamp(value, integer{0}, integer{15})) << shift);
    }

    const vector<Volatile>& Volatile::table() {
        static const vector<Volatile> table{
            Volatile{.name = "Water", .molarMass = 18.015f, .freezeKelvin = 273.15f, .boilKelvin = 373.15f, .greenhouse = 1.00f, .scatter = vec3{0.62f, 0.78f, 1.05f}, .absorb = vec3{0.02f, 0.03f, 0.05f}},
            Volatile{.name = "CarbonDioxide", .molarMass = 44.010f, .freezeKelvin = 194.67f, .boilKelvin = 216.58f, .greenhouse = 0.72f, .scatter = vec3{0.48f, 0.78f, 1.35f}, .absorb = vec3{0.03f, 0.04f, 0.08f}},
            Volatile{.name = "Nitrogen", .molarMass = 28.014f, .freezeKelvin = 63.15f, .boilKelvin = 77.36f, .greenhouse = 0.04f, .scatter = vec3{0.38f, 0.92f, 2.45f}, .absorb = vec3{0.00f, 0.00f, 0.00f}},
            Volatile{.name = "Methane", .molarMass = 16.043f, .freezeKelvin = 90.69f, .boilKelvin = 111.66f, .greenhouse = 0.92f, .scatter = vec3{0.82f, 0.52f, 0.22f}, .absorb = vec3{0.10f, 0.18f, 0.42f}},
            Volatile{.name = "Ammonia", .molarMass = 17.031f, .freezeKelvin = 195.40f, .boilKelvin = 239.81f, .greenhouse = 0.68f, .scatter = vec3{0.88f, 0.84f, 0.70f}, .absorb = vec3{0.05f, 0.06f, 0.10f}},
            Volatile{.name = "SulfurDioxide", .molarMass = 64.066f, .freezeKelvin = 197.67f, .boilKelvin = 263.05f, .greenhouse = 0.56f, .scatter = vec3{0.92f, 0.78f, 0.36f}, .absorb = vec3{0.08f, 0.16f, 0.32f}},
            Volatile{.name = "Hydrogen", .molarMass = 2.016f, .freezeKelvin = 13.99f, .boilKelvin = 20.27f, .greenhouse = 0.02f, .scatter = vec3{0.50f, 0.85f, 1.65f}, .absorb = vec3{0.00f, 0.00f, 0.02f}},
            Volatile{.name = "Helium", .molarMass = 4.003f, .freezeKelvin = 0.95f, .boilKelvin = 4.22f, .greenhouse = 0.00f, .scatter = vec3{0.58f, 0.82f, 1.28f}, .absorb = vec3{0.00f, 0.00f, 0.00f}},
        };
        return table;
    }

}
