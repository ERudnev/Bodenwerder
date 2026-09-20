#pragma once

#include "geo/details/icosaMap.h"
#include "geo/details/weather.h"

#include <base/maybe.h>
#include <eltanin/geo/minerals.q1.h>
#include <eltanin/geo/volatiles.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/patchGrid.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::planet {

    using namespace fqsm::api;

    struct Passport {
        integer seed;
        float ageGyr;
        double mass;
        float radius;
        geo::Mineral::Mix bulk;
        geo::Volatile::Mix volatiles;
        struct {
            vec3 axis;
            float period;
        } spin;
        struct {
            float stellarFlux; // W/m², orbit-mean
            float eccentricity;
            float tidalHeat; // W/m² deposited inside the body
            float debrisFlux; // normalized surviving impactor population
        } environment;
    };

    struct Planet {
        struct Probe {
            double height;
            dvec3 position;
            dvec3 normal;
            geo::Mineral::Mix mix;
            float slope;
        };

        struct Detail {
            integer edgeBase;
            integer tessellation;
        };

        struct Runtime {
            float surfaceAcceleration;
            float reliefAmplitude;
            struct {
                float outerRadius;
                float seaDensity;
                float kerman;
                float zenithTau;
                rmmr::RGB day;
            } atmosphere;
        };

        const Passport passport;
        Runtime runtime;
        dvec3 spinOmega; // world angular velocity; the planet is translationally fixed
        base::maybe<phys::Body::Id> well;
        geo::IcosaMap<std::int16_t> heights; // 0 = sea; ±reliefPeak maps to ±amplitude metres
        geo::IcosaMap<std::uint16_t> covers; // two u8 facies: surface, then just below
        geo::IcosaMap<rmmr::vec4> farAlbedo; // filtered RGBA: mean albedo + roughness
        geo::IcosaMap<rmmr::vec3> farNormal; // object-space unit normal, far-albedo grid
        base::maybe<Weather> weather;
        struct {
            bool atmosphere;
            bool fog;
        } draw;

    private:
        base::maybe<rmmr::scene::actor::PatchGrid::Id> shell;
        base::maybe<rmmr::scene::actor::Mesh::Id> atmosphere;

    public:
        static constexpr float constructionEdge = 4.0f; // construct cubes, metres
        static constexpr std::int16_t reliefPeak = 32767;

        static auto recommendedDetail(float radius, float edge) -> Detail;

        Planet(Passport, Detail);

        void place(Writing, rmmr::system::Device::Id, rmmr::Pose);
        void update(Writing, rmmr::Pos camera);
        void advancePhysics(Writing, seconds dt);
        void sync(Writing);

        auto spin(const phys::Body::Quantum&) const -> float;
        void spin(phys::Body::Quantum&, float) const;
        auto reliefScale() const -> float; // metres per int16 step
        auto surfaceRadius(std::int16_t quantum) const -> double;
        auto encodeRelief(float deltaMeters) const -> std::int16_t;
        auto height(rmmr::vec3 dir) const -> double; // radial surface, not relief above sea
        auto altitudeAt(const phys::Body::Quantum&, dvec3 worldPos) const -> double;
        auto gravityAt(const phys::Body::Quantum&, dvec3 worldPos) const -> dvec3;
        auto airDensity(const phys::Body::Quantum&, dvec3 worldPos) const -> float;
        auto windAt(const phys::Body::Quantum&, dvec3 worldPos) const -> dvec3;
        auto probe(const phys::Body::Quantum&, rmmr::vec3 dir) const -> Probe;
        auto surfaceAt(const phys::Body::Quantum&, dvec3 worldPos) const -> Probe;
    };

}
