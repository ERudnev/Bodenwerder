#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::geo {

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
        static const vector<Mineral>& table();
    };

}
