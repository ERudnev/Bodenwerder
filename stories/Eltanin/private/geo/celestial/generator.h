#pragma once

#include "geo/celestial/planet.h"

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    struct Generator {
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

            Formation(geo::IcosaPack surface, geo::IcosaPack features)
                : relief{surface, 0.0f}
                , province{features, 0.0f}
                , composition{features, 0.0f}
                , crustAge{features, 0.0f}
                , boundary{features, 0.0f}
                , volcanic{features, 0.0f}
                , impact{features, 0.0f}
                , fracture{features, 0.0f}
                , sediment{features, 0.0f}
                , water{features, 0.0f}
                , exogenic{features, 0.0f} {
            }
        };

        struct PlateSite {
            vec3 center;
            vec3 pole;
            float speed;
            float elevation;
            float age;
            float felsic;
        };

        struct PlateField {
            vector<PlateSite> sites;
            integer seed;
            float amplitude;
            float width;
            float activity;
        };

        struct Basin {
            vec3 center;
            vec3 along;
            float radius;
            float depth;
            float obliquity;
            float exogenic;
            integer seed;
        };

        struct Provinces {
            integer count;
            integer seed;
            float amplitude;
        };

        struct Burst {
            vec3 axis;
            float radius;
            float lift;
            float rim;
            integer seed;
        };

        struct Swell {
            vec3 axis;
            float sigma;
            float amplitude;
            integer seed;
        };

        struct Rift {
            vec3 center;
            vec3 along;
            float halfWidth;
            float halfLength;
            float depth;
            integer seed;
        };

        struct Erode {
            float years;
            float strength;
            float north;
            integer iterations;
            integer seed;
        };

        struct Drainage {
            integer seed;
            integer sources;
            integer steps;
            float stepLength;
            float width;
            float depth;
        };

        struct Bombardment {
            integer seed;
            integer count;
            float radiusMin;
            float radiusMax;
            float depth;
            float northDensity;
        };

        struct Rub {
            vec3 a;
            vec3 b;
            float slip;
            float width;
            integer seed;
        };

        struct Whisper {
            integer seed;
            float freq;
            float amplitude;
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
        };

        static void generate(Planet&);
        static void mars(Planet&);

    private:
        static auto derive(const Passport&) -> Geology;
        static void form(Planet&, const Geology&);
        static void applyPlateField(Formation&, const PlateField&);
        static void applyBasin(Formation&, const Basin&);
        static void stampVolcanic(geo::IcosaMap<float>&, vec3 axis, float radius, float amount, integer seed);
        static void applyProvinces(geo::IcosaMap<float>&, const Provinces&);
        static void applyBurst(geo::IcosaMap<float>&, const Burst&);
        static void applyBursts(geo::IcosaMap<float>&, const vector<Burst>&);
        static void applySwell(geo::IcosaMap<float>&, const Swell&);
        static void applyRift(geo::IcosaMap<float>&, const Rift&);
        static void applyErode(geo::IcosaMap<float>&, const Erode&);
        static void applyDrainage(geo::IcosaMap<float>&, const Drainage&);
        static void applyBombardment(geo::IcosaMap<float>&, const Bombardment&);
        static void applyRub(geo::IcosaMap<float>&, const Rub&);
        static void applyWhisper(geo::IcosaMap<float>&, const Whisper&);
        static void paintCover(Planet&, const PaintCover&);
        static void paintFormation(Planet&, const Formation&, const Geology&);
        static auto marsCover(const Geology&, integer seed) -> PaintCover;
    };

}
