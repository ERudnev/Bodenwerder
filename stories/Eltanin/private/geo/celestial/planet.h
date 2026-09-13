#pragma once

#include "geo/details/icosaPack.h"

#include <base/maybe.h>
#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

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

        const Passport passport;
        rmmr::Pose pose;
        float spin; // radians around local +Y
        base::maybe<phys::Body::Id> well;

    private:
        geo::IcosaPack pack;
        vector<float> heights;
        base::maybe<rmmr::scene::actor::Mesh::Id> shell;
        base::maybe<rmmr::scene::actor::Mesh::Id> atmosphere;

    public:
        Planet(Passport);

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
