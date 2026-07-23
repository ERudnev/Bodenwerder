#pragma once

#include <filesystem>
#include <vector>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

namespace toy::assets {

    // Toy's curated asset set (ids for now; References later).
    struct Handles {
        struct {
            std::vector<rmmr::resource::texture::Asset::Id> debug;
            base::maybe<rmmr::resource::texture::Asset::Id> whiteCircle;
            base::maybe<rmmr::resource::texture::Asset::Id> whiteRing;
        } texture;
        struct {
            base::maybe<rmmr::resource::material::Asset::Id> ambient;
            base::maybe<rmmr::resource::material::Asset::Id> lit;
            std::vector<rmmr::resource::material::Asset::Id> debugLitTextured;
            base::maybe<rmmr::resource::material::Asset::Id> litTexturedAlpha;
            base::maybe<rmmr::resource::material::Asset::Id> grid;
        } material;
        struct {
            base::maybe<rmmr::resource::geometry::Asset::Id> triangle;
            base::maybe<rmmr::resource::geometry::Asset::Id> kube;
            base::maybe<rmmr::resource::geometry::Asset::Id> bagel;
            base::maybe<rmmr::resource::geometry::Asset::Id> grid;
        } primitive;
    };

    // Owns Toy asset catalogue lifecycle: seed / load / save.
    // Remap of Handles after load — next step.
    struct Manager {
        using Location = std::filesystem::path;

        Handles handles;

        void hardcodedInit(fqsm::Writing, rmmr::system::Core::Id);
        bool loadFrom(fqsm::Stewarding, Location);
        void save(fqsm::Reading, Location);
    };

}
