#pragma once

#include <span>

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"
#include "fittings/mounts/catalog.h"
#include "physics/system.h"
#include "scenarios/asterField.h"
#include "story.ui.h"
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
            base::maybe<rmmr::resource::geometry::Asset::Id> scrap;
            base::maybe<rmmr::resource::material::Asset::Id> skySphereMaterial;
            base::maybe<rmmr::resource::texpack::Pack::Id> sprites;
            base::maybe<rmmr::resource::texpack::Pack::Id> mech;
            base::maybe<rmmr::resource::meshpack::Asset::Id> interframe;
            base::maybe<rmmr::resource::meshpack::Asset::Id> attachments;
            base::maybe<rmmr::resource::meshpack::Asset::Id> armour;
            base::maybe<rmmr::resource::meshpack::Asset::Id> devices;
            base::maybe<rmmr::resource::meshpack::Asset::Id> projectiles;
            base::maybe<rmmr::resource::overlay::Asset::Id> blueprintsEditorEffect;
            base::maybe<rmmr::resource::material::Asset::Id> collisionDebugMaterial;
        };

        Handles assets;
        Ui ui;
        base::maybe<View> world_view;
        base::maybe<phys::System> physics;
        scenario::AsterField scenario;
        BlueprintCatalog blueprintPack;
        MountCatalog mountPack;
        ::eltanin::views::Blueprints blueprints;

        Schema schema() const override;
        void createCore(Writing) override;
        void addAssets(Writing) override;
        void prepareAssets(Writing) override;
        void setup(Writing, rmmr::system::Window::Id) override;
        void onFrame(establish::Realm&, int64 dt_us) override;
        void contributeViewMenu(Writing) override;
        void drawUi(Writing) override;
        auto activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> override;
        auto overlaySelection() const -> std::span<const rmmr::renderer::Integer32> override;

    private:
        void populateWorld(Writing, rmmr::system::Window::Id);
        void bindGameEntities(Writing);
        void advanceSim(Writing, seconds dt);
        void drawCameraWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
        void drawAssemblerWindow(Writing);
    };

}
