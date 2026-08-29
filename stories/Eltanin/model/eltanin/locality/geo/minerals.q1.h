#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    struct Mineral {
        using Index = integer;
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

        static const vector<Mineral>& table();
    };

}
