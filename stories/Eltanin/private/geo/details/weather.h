#pragma once

#include "geo/details/icosaMap.h"

#include <base/maybe.h>
#include <eltanin/geo/minerals.q1.h>
#include <eltanin/geo/volatiles.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::planet {

    using namespace fqsm::api;
    using namespace rmmr;

    struct Geology;
    struct Planet;

    struct Weather {
        enum class Kind : integer {
            Dust,
            Condensate,
            Haze,
        };

        struct Deck {
            Kind kind;
            float base;
            float top;
            RGB scatter;
            float channel;
            base::maybe<rmmr::scene::actor::Mesh::Id> actor;
        };

        geo::Mineral::Mix crust;
        geo::Volatile::Mix retained;
        float cohesion;
        float water;
        float ice;
        float column;
        float temperature;
        vector<Deck> decks;
        geo::IcosaMap<float> heat;
        geo::IcosaMap<vec2> wind;
        geo::IcosaMap<float> cloud;
        geo::IcosaMap<float> dust;
        seconds debt;
        RGB baseDay;
        float baseTau;
        vec3 dustTint;
        base::maybe<rmmr::resource::texture::Asset::Id> atlas;

        static auto decksOf(const Geology&, float kerman) -> vector<Deck>;
        static auto spawn(const Geology&, integer heightSegments, float kerman, float seaDensity) -> base::maybe<Weather>;

        void tick(vec3 sunLocal, float stellarFlux, seconds dt);
        auto atlasPixels() const -> vector<std::uint8_t>;
        auto meanDust() const -> float;
        auto meanCloud() const -> float;
        auto meanWind() const -> float;
        auto windAt(vec3 direction) const -> vec2;
        void applyLook(Planet&) const;
    };

}
