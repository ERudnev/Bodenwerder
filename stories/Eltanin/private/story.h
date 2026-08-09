#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/wrapper/product.h>

#include "physics/system.h"
#include "physics/ui.h"
#include "views/blueprints/editor.h"

namespace eltanin {

    using namespace fqsm::api;

    class Game : public rmmr::wrapper::Product {
    public:
        struct Handles {
            struct {
                base::maybe<rmmr::resource::geometry::Asset::Id> grid;
                base::maybe<rmmr::resource::geometry::Asset::Id> sphere;
                base::maybe<rmmr::resource::geometry::Asset::Id> kube;
                base::maybe<rmmr::resource::geometry::Asset::Id> diamond;
            } primitive;
            base::maybe<rmmr::resource::geometry::Asset::Id> skySphereGeometry;
            base::maybe<rmmr::resource::material::Asset::Id> skySphereMaterial;
            base::maybe<rmmr::resource::texpack::Pack::Id> sprites;
            base::maybe<rmmr::resource::texpack::Pack::Id> mech;
            base::maybe<rmmr::resource::meshpack::Asset::Id> levelOne;
            base::maybe<rmmr::resource::meshpack::Asset::Id> levelTwo;
            base::maybe<rmmr::resource::meshpack::Asset::Id> interframe;
            base::maybe<rmmr::resource::overlay::Asset::Id> blueprintsEditorEffect;
        };

        struct Ui {
            bool camera = false;
            bool lighting = false;
            bool materials = false;
            bool physics = false;
            bool blueprints = true;

            base::maybe<rmmr::resource::material::Asset::Id> selected_material;
            std::array<char, 128> material_filter{};

            struct MaterialNameEdit {
                std::array<char, 256> buf{};
                bool editing = false;
            };
            std::unordered_map<std::uint64_t, MaterialNameEdit> material_name_edits;
        };

        Handles assets;
        Ui ui;
        base::maybe<View> world_view;
        phys::System physics;
        phys::Ui physics_ui;
        ::eltanin::views::Blueprints blueprints;

        Schema schema() const override;
        void createCore(Writing) override;
        void addAssets(Writing) override;
        void prepareAssets(Writing) override;
        void setup(Writing, rmmr::system::Window::Id) override;
        void onFrame(establish::Realm&, int64 dt_us) override;
        void contributeViewMenu() override;
        void drawUi(Writing) override;
        auto activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> override;
        auto overlaySelection() const -> std::span<const rmmr::renderer::Integer32> override;

    private:
        void populateWorld(Writing, rmmr::system::Window::Id);
        void advanceSim(Writing, int64 dt_us);
        void drawCameraWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
    };

}
