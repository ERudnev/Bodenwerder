#pragma once

#include "geo/details/icosaMap.h"

#include <base/maybe.h>
#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/patchGrid.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::locality::planet {

    using namespace fqsm::api;

    struct Passport {
        integer seed;
        float radius;
        float surfaceAcceleration;
        quat orientation; // idle attitude in solar-system space; local +Y is the spin pole
        float spinPeriod; // seconds per revolution; 0 = no automatic spin
        struct Geology {
            geo::Mineral::Mix mix;
            float differentiation; // 0 unsorted boulder, 1 heavies sank to core
            float surfaceAge; // 0 fresh melt, 1 ancient crust
            float cohesion; // 0 crumbling, 1 fused monolith
            float grain;
            float tectonic;
            float amplitude; // metres from sea, both ways; ±int16 full scale; collision sphere is radius + amplitude
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

        const Passport passport;
        dvec3 spinOmega; // world angular velocity; the planet is translationally fixed
        base::maybe<phys::Body::Id> well;
        geo::IcosaMap<std::int16_t> heights; // 0 = sea; ±reliefPeak maps to ±amplitude metres
        geo::IcosaMap<std::uint16_t> covers; // two u8 facies: surface, then just below
        geo::IcosaMap<rmmr::vec4> farAlbedo; // filtered RGBA: mean albedo + roughness

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
