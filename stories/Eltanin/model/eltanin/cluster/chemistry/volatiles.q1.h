#pragma once

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::cluster::chemistry {

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
            Oxygen,
            Carbon,
            Silicon,
            Iron,
        };
        static constexpr integer kindCount = 12;
        using Mix = std::uint64_t;
        string name;
        float molarMass;
        float freezeKelvin;
        float boilKelvin;
        float greenhouse;
        vec3 scatter;
        vec3 absorb;

        static auto nibble(Mix mix, Kind kind) -> integer { return integer((mix >> (static_cast<integer>(kind) * 4)) & 15u); }
        static auto pack(Mix mix, Kind kind, integer value) -> Mix;

        static const vector<Volatile>& table();
    };

}
