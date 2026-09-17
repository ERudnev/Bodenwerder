#include <eltanin/geo/volatiles.q1.h>

namespace eltanin::geo {

    const vector<Volatile>& Volatile::table() {
        static const vector<Volatile> table{
            Volatile{.name = "Water", .molarMass = 18.015f, .freezeKelvin = 273.15f, .boilKelvin = 373.15f, .greenhouse = 1.00f},
            Volatile{.name = "CarbonDioxide", .molarMass = 44.010f, .freezeKelvin = 194.67f, .boilKelvin = 216.58f, .greenhouse = 0.72f},
            Volatile{.name = "Nitrogen", .molarMass = 28.014f, .freezeKelvin = 63.15f, .boilKelvin = 77.36f, .greenhouse = 0.04f},
            Volatile{.name = "Methane", .molarMass = 16.043f, .freezeKelvin = 90.69f, .boilKelvin = 111.66f, .greenhouse = 0.92f},
            Volatile{.name = "Ammonia", .molarMass = 17.031f, .freezeKelvin = 195.40f, .boilKelvin = 239.81f, .greenhouse = 0.68f},
            Volatile{.name = "SulfurDioxide", .molarMass = 64.066f, .freezeKelvin = 197.67f, .boilKelvin = 263.05f, .greenhouse = 0.56f},
            Volatile{.name = "Hydrogen", .molarMass = 2.016f, .freezeKelvin = 13.99f, .boilKelvin = 20.27f, .greenhouse = 0.02f},
            Volatile{.name = "Helium", .molarMass = 4.003f, .freezeKelvin = 0.95f, .boilKelvin = 4.22f, .greenhouse = 0.00f},
        };
        return table;
    }

}
