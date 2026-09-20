#pragma once

#include "geo/celestial/planet.h"
#include "geo/details/effects.h"

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    struct Geology {
        struct Crust {
            geo::Mineral::Mix mix;
            integer plates;
            float differentiation;
            float thickness;
            float mobility;
            float fragmentation;
            float cohesion;
            float grain;
        } crust;
        struct Mantle {
            float heat;
            float plumeRate;
            float plumePower;
            float boundaryAffinity;
        } mantle;
        struct Bombardment {
            geo::Mineral::Mix mix;
            float flux;
            float violence;
            float largeBodyTail;
            float ironFraction;
        } bombardment;
        struct Climate {
            geo::Volatile::Mix retained;
            float atmosphere;
            float temperature;
            float water;
            float ice;
            float weathering;
            float transport;
        } climate;
        struct History {
            float surfaceAge;
            float reliefAmplitude;
        } history;
    };

    struct AtmosphereLook {
        RGB day;
        float zenithTau;
        static auto of(const Geology&) -> AtmosphereLook;
    };

    struct Formation {
        geo::IcosaMap<float> relief;
        geo::IcosaMap<float> province;
        geo::IcosaMap<float> composition;
        geo::IcosaMap<float> crustAge;
        geo::IcosaMap<float> boundary;
        geo::IcosaMap<float> volcanic;
        geo::IcosaMap<float> impact;
        geo::IcosaMap<float> fracture;
        geo::IcosaMap<float> sediment;
        geo::IcosaMap<float> water;
        geo::IcosaMap<float> exogenic;

        Formation(geo::IcosaPack surface, geo::IcosaPack features);
    };

    struct PaintCover {
        integer ice;
        integer olivine;
        integer pyroxene;
        integer feldspar;
        integer clay;
        integer carbonaceous;
        integer iron;
        integer oxides;
        integer salts;
        float cohesion;
        float age;
        float differentiation;
        vec3 tharsis;
        vec3 olympus;
        vec3 canyonCenter;
        vec3 canyonAlong;
        float canyonHalfWidth;
        float canyonHalfLength;
        integer seed;

        static auto mars(const Geology&, integer seed) -> PaintCover;
        static void apply(Planet&, const PaintCover&);
    };

    struct Compose {
        static auto derive(const Passport&) -> Geology;
        static void form(Planet&, const Geology&);
        static void paint(Planet&, const Formation&, const Geology&);
    };

}
