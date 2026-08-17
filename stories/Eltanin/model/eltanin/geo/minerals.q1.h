#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::geo {

    using namespace fqsm::api;

    struct Mineral {
        string name;
        float density;
        float scale;
        vec3 albedo;
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
