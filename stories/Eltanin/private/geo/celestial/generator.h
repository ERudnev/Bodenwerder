#pragma once

#include "geo/celestial/planet.h"

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    struct Generator {
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
        static void generateSurfaceWeights(Planet&);
        static void mars(Planet&);

    private:
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
        static auto marsCover(const Passport&) -> PaintCover;
    };

}
