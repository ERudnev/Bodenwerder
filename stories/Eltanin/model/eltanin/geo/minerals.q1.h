#pragma once

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::geo {

    using namespace fqsm::api;

    struct Mineral {
        enum class Kind : integer {
            Ice,
            Olivine,
            Pyroxene,
            Feldspar,
            Clay,
            Carbonaceous,
            Iron,
            Nickel,
            Sulfides,
            Oxides,
            BaseMetals,
            PGM,
            RareEarths,
            Actinides,
            Salts,
            Exotic,
        };
        using Index = integer;
        using Mix = std::uint64_t;
        string name;
        float density;
        float scale;
        vec3 albedo;
        vec3 sinter;
        float roughness;
        float metalness;
        float hardness;
        float meltKelvin;
        float tintKelvin;
        float glowKelvin;
        float sootMul;

        auto kgPerCubicMeter() const -> float { return density * 1000.0f; }
        static auto nibble(Mix mix, Kind kind) -> integer { return integer((mix >> (static_cast<integer>(kind) * 4)) & 15u); }

        static const vector<Mineral>& table();
    };

}
