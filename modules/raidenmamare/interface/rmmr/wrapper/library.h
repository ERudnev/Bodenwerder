#pragma once

#include <filesystem>
#include <vector>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/textures.q1.h>

namespace rmmr::wrapper::assets {

    using namespace fqsm::api;

    struct Handles {
        struct {
            base::maybe<rmmr::resource::texpack::Pack::Id> debug;
            base::maybe<rmmr::resource::texture::Asset::Id> whiteCircle;
            base::maybe<rmmr::resource::texture::Asset::Id> whiteRing;
        } texture;
        base::maybe<rmmr::resource::meshpack::Asset::Id> primitives;
        struct {
            struct {
                base::maybe<rmmr::resource::material::Asset::Id> textured;
                base::maybe<rmmr::resource::material::Asset::Id> vertexColor;
            } gizmo;
            base::maybe<rmmr::resource::material::Asset::Id> ambient;
            base::maybe<rmmr::resource::material::Asset::Id> lit;
            base::maybe<rmmr::resource::material::Asset::Id> litTransparent;
            base::maybe<rmmr::resource::material::Asset::Id> litTextured;
            base::maybe<rmmr::resource::material::Asset::Id> litTexturedAlpha;
            base::maybe<rmmr::resource::material::Asset::Id> oneSidedGlass;
            base::maybe<rmmr::resource::material::Asset::Id> grid;
            base::maybe<rmmr::resource::material::Asset::Id> sprite;
            base::maybe<rmmr::resource::material::Asset::Id> identity;
        } material;
        struct {
            base::maybe<rmmr::resource::overlay::Asset::Id> defaultBlur;
        } overlay;
    };

    enum class PrepareStatus {
        Generated,
        Loaded,
        Failed,
    };

    struct Manager {
        using Location = std::filesystem::path;

        Handles handles;

        static auto statePath(const std::filesystem::path& assets_root) -> Location;

        auto prepare(Writing, Location) -> PrepareStatus;
        void addHardcoded(Writing);
        bool loadFrom(Stewarding, Location);
    };

}
