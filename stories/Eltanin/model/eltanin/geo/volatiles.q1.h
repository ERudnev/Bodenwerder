#pragma once

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::geo {

    using namespace fqsm::api;

    struct Volatile {
        enum class Kind : integer {
            Water,
            CarbonDioxide,
            Nitrogen,
            Methane,
            Ammonia,
            SulfurDioxide,
            Hydrogen,
            Helium,
        };
        using Mix = std::uint32_t;
        string name;
        float molarMass;
        float freezeKelvin;
        float boilKelvin;
        float greenhouse;

        static const vector<Volatile>& table();
    };

}
