#pragma once

#include <filesystem>
#include <vector>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>

namespace rmmr::wrapper::assets {

    using namespace fqsm::api;

    // Toy's curated asset set (ids for now; References later).
    struct Handles {
        struct {
            std::vector<rmmr::resource::texture::Asset::Id> debug;
            base::maybe<rmmr::resource::texture::Asset::Id> whiteCircle;
            base::maybe<rmmr::resource::texture::Asset::Id> whiteRing;
        } texture;
        struct {
            struct {
                base::maybe<rmmr::resource::material::Asset::Id> textured;
                base::maybe<rmmr::resource::material::Asset::Id> vertexColor;
            } gizmo;
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

    // Toy assets: addHardcoded / loadFrom / save. Disk catalogue archived; live path is addHardcoded.
    struct Manager {
        using Location = std::filesystem::path;

        Handles handles;

        static auto statePath(const std::filesystem::path& assets_root) -> Location;

        auto prepare(Writing, Location) -> PrepareStatus;
        void addHardcoded(Writing);
        bool loadFrom(Stewarding, Location);
        void save(Writing, Location);
    };

}
