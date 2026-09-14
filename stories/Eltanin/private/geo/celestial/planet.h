#pragma once

#include "geo/details/icosaMap.h"

#include <base/maybe.h>
#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::locality::planet {

    using namespace fqsm::api;

    struct Passport {
        integer seed;
        float radius;
        float surfaceAcceleration;
        quat orientation; // body frame at epoch; local +Y is the spin pole
        struct Geology {
            geo::Mineral::Mix mix;
            float differentiation; // 0 unsorted boulder, 1 heavies sank to core
            float surfaceAge; // 0 fresh melt, 1 ancient crust
            float cohesion; // 0 crumbling, 1 fused monolith
            float grain;
            float tectonic;
            float maxRelief; // guaranteed, may be used as rough collision sphere radius
        } geology;
        struct Atmosphere {
            float outerRadius;
            float seaDensity;
            float kerman;
            rmmr::RGB day;
        } atmosphere;
    };

    struct Planet {
        struct Probe {
            float height;
            dvec3 position;
            dvec3 normal;
            geo::Mineral::Mix mix;
            float slope;
        };

        struct Detail {
            integer edgeBase;
            integer tessellation;
        };

        const Passport passport;
        rmmr::Pose pose;
        float spin; // radians around local +Y
        base::maybe<phys::Body::Id> well;
        geo::IcosaMap<float> heights;
        geo::IcosaMap<rmmr::RGB> colors;
        geo::IcosaMap<std::uint32_t> covers; // 4×8bit facies layers, shallow → deep

    private:
        base::maybe<rmmr::scene::actor::Mesh::Id> shell;
        base::maybe<rmmr::scene::actor::Mesh::Id> atmosphere;

    public:
        static constexpr float constructionEdge = 4.0f; // construct cubes, metres

        static auto recommendedDetail(float radius, float edge) -> Detail;

        Planet(Passport, Detail);

        void place(Writing, rmmr::system::Device::Id, rmmr::Pose);
        void update(Writing, rmmr::Pos camera);
        void sync(Writing);

        auto height(rmmr::vec3 dir) const -> float; // radial surface, not relief above sea
        auto altitudeAt(rmmr::Pos worldPos) const -> float;
        auto gravityAt(dvec3 worldPos) const -> dvec3;
        auto airDensity(rmmr::Pos) const -> float;
        auto windAt(dvec3) const -> dvec3;
        auto probe(rmmr::vec3 dir) const -> Probe;
    };

}
