#pragma once

#include "geo/details/icosaMap.h"

#include <cstddef>
#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    struct Formation;

    struct Sample {
        static auto hash01(integer x, integer y, integer z, integer salt) -> float;
        static auto sphereDir(integer index, integer seed, integer saltU, integer saltV) -> vec3;
        static auto noise(vec3 point, integer seed) -> float;
        static auto fractal(vec3 point, integer seed, integer octaves, float persistence) -> float;
        static auto ridged(vec3 point, integer seed, integer octaves) -> float;
        static auto warped(vec3 direction, integer seed, float frequency, float strength) -> vec3;
        static auto angular(vec3 a, vec3 b) -> float;
        static auto gaussian(float angle, float sigma) -> float;

    private:
        static auto valueNoise(float x, float y, float z, integer seed) -> float;
    };

    struct SpatialHash {
        static constexpr integer span = 32;
        struct Cell {
            integer x;
            integer y;
            integer z;
        };
        static auto cell(vec3 direction) -> Cell;
        static auto index(integer x, integer y, integer z) -> std::size_t;
    };

    struct PlateSite {
        vec3 center;
        vec3 pole;
        float speed;
        float elevation;
        float age;
        float felsic;

        struct Hit {
            integer first;
            integer second;
            float firstDot;
            float secondDot;
        };
        static auto hit(vec3 direction, const vector<PlateSite>&) -> Hit;
    };

    struct PlateField {
        vector<PlateSite> sites;
        integer seed;
        float amplitude;
        float width;
        float activity;

        struct Hit {
            integer first;
            integer second;
            float firstScore;
            float secondScore;
            vec3 normal;
            vec3 along;
            float divergence;
            float shear;
        };
        auto sample(vec3 direction) const -> Hit;
        static void apply(Formation&, const PlateField&);
    };

    struct Basin {
        vec3 center;
        vec3 along;
        float radius;
        float depth;
        float obliquity;
        float exogenic;
        integer seed;
        static void apply(Formation&, const Basin&);
    };

    struct Provinces {
        integer count;
        integer seed;
        float amplitude;
        static void apply(geo::IcosaMap<float>&, const Provinces&);
    };

    struct Burst {
        vec3 axis;
        float radius;
        float lift;
        float rim;
        integer seed;

        struct Epoch {
            integer seed;
            integer salt;
            integer count;
            float radiusMin;
            float radiusSpan;
            float depthMin;
            float depthSpan;
            float highland;
            vec3 avoidAxis;
            float avoidDot;
        };
        static auto impact(vec3 axis, float radius, float depth, integer seed) -> Burst;
        static auto eruption(vec3 axis, float radius, float height, integer seed) -> Burst;
        static auto epoch(const Epoch&) -> vector<Burst>;
        static auto delta(vec3 direction, const Burst&) -> float;
        static void apply(geo::IcosaMap<float>&, const Burst&);
        static void apply(geo::IcosaMap<float>&, const vector<Burst>&);
    };

    struct Swell {
        vec3 axis;
        float sigma;
        float amplitude;
        integer seed;
        static void apply(geo::IcosaMap<float>&, const Swell&);
    };

    struct Rift {
        vec3 center;
        vec3 along;
        float halfWidth;
        float halfLength;
        float depth;
        integer seed;
        static void apply(geo::IcosaMap<float>&, const Rift&);
    };

    struct Erode {
        float years;
        float strength;
        float north;
        integer iterations;
        integer seed;
        static void apply(geo::IcosaMap<float>&, const Erode&);
    };

    struct Drainage {
        integer seed;
        integer sources;
        integer steps;
        float stepLength;
        float width;
        float depth;

        struct Source {
            vec3 direction;
            float height;
        };
        struct Channel {
            vec3 direction;
            float width;
            float depth;
        };
        static void apply(geo::IcosaMap<float>&, const Drainage&);
    };

    struct Bombardment {
        integer seed;
        integer count;
        float radiusMin;
        float radiusMax;
        float depth;
        float northDensity;
        static void apply(geo::IcosaMap<float>&, const Bombardment&);
    };

    struct Rub {
        vec3 a;
        vec3 b;
        float slip;
        float width;
        integer seed;
        static void apply(geo::IcosaMap<float>&, const Rub&);
    };

    struct Whisper {
        integer seed;
        float freq;
        float amplitude;
        static void apply(geo::IcosaMap<float>&, const Whisper&);
    };

    struct Volcanic {
        static void stamp(geo::IcosaMap<float>&, vec3 axis, float radius, float amount, integer seed);
    };

}
