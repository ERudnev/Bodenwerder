#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/physical.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/wrapper/product.h>

#include "physics.h"

namespace kubes {

    using namespace fqsm::api;

    class KubeOfKubes : public rmmr::wrapper::Product {
    public:
        struct Assets {
            struct {
                base::maybe<rmmr::resource::geometry::Asset::Id> grid;
                base::maybe<rmmr::resource::geometry::Asset::Id> sphere;
                base::maybe<rmmr::resource::geometry::Asset::Id> unitQuad;
            } primitive;
            base::maybe<rmmr::resource::sprite::Pack::Id> skySphere;
            base::maybe<rmmr::resource::geometry::Asset::Id> skySphereGeometry;
            base::maybe<rmmr::resource::material::Asset::Id> skySphereMaterial;
            base::maybe<rmmr::resource::physical::Asset::Id> unitCube;
        };

        struct Ui {
            bool camera = false;
            bool lighting = false;
            bool materials = false;
            bool physics = true;

            base::maybe<rmmr::resource::material::Asset::Id> selected_material;
            std::array<char, 128> material_filter{};

            struct MaterialNameEdit {
                std::array<char, 256> buf{};
                bool editing = false;
            };
            std::unordered_map<std::uint64_t, MaterialNameEdit> material_name_edits;
        };

        Assets assets;
        Ui ui;
        phys::System physics;

        Schema schema() const override;
        void addAssets(Writing) override;
        void setup(establish::Realm&, rmmr::system::Window::Id) override;
        void onFrame(establish::Realm&, int64 dt_us) override;
        void contributeViewMenu() override;
        void drawUi(Writing) override;

    private:
        void populateWorld(Writing, rmmr::system::Window::Id);
        void advanceSim(Writing, int64 dt_us);
        void drawCameraWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
    };

}
