#pragma once

#include <filesystem>
#include <vector>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

namespace toy::assets {

    using namespace fqsm::api;

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
            base::maybe<rmmr::resource::material::Asset::Id> sprite;
        } material;
    };

    enum class PrepareStatus {
        Generated,
        Loaded,
        Failed,
    };

    // Owns Toy asset catalogue lifecycle: seed / load / save.
    // Load/save of on-disk catalogue archived with Retrospection; live path is hardcoded seed.
    struct Manager {
        using Location = std::filesystem::path;

        const rmmr::system::Core::Id core;
        Handles handles;

        static auto statePath(const std::filesystem::path& assets_root) -> Location;

        auto prepare(establish::Realm&, Location) -> PrepareStatus;
        void hardcodedInit(Writing);
        bool loadFrom(Stewarding, Location);
        void save(Writing, Location);
    };

}
