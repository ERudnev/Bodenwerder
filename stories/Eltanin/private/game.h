#pragma once

#include <span>
#include <vector>

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"
#include "fittings/mounts/catalog.h"
#include "geo/celestial/planet.h"
#include "physics/system.h"
#include "resources/library.h"
#include "scenarios/planeliod.h"
#include "scenarios/strategic.h"
#include "strategic/map.h"
#include "locality.ui.h"
#include "views/blueprints/editor.h"
#include "views/starMap/view.h"

namespace eltanin {

    using namespace fqsm::api;

    struct Focus {
        vector<locality::Thing::Id> things;
    };

    class Game : public rmmr::wrapper::Product {
    public:
        enum class UiMode {
            starMap,
            locality,
        };

        struct Cameras {
            enum class Kind { free, spectator };
            Kind kind;
            rmmr::scene::Camera::Id free;
            rmmr::scene::Camera::Id spectator;
            rmmr::system::ViewInput::Id freeInput;
            rmmr::system::ViewInput::Id spectatorInput;
            bool hotkeyDown;
        };

        ::eltanin::assets::Handles assets;
        Ui ui;
        base::maybe<View> world_view;
        base::maybe<phys::System> physics;
        base::maybe<planet::Planet> planet;
        scenario::Strategic strategic;
        scenario::Planeliod planeliod;
        strategic::Map map;
        views::starmap::View starMap;
        UiMode uiMode;
        Focus focus;
        base::maybe<Cameras> cameras;
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
        void contributeLocalityMenu(Writing);
        void drawLocalityUi(Writing);
        auto activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> override;
        auto overlaySelection() const -> std::span<const rmmr::renderer::Integer32> override;

    private:
        void populateWorld(Writing, rmmr::system::Window::Id);
        void bindGameEntities(Writing);
        void advanceSim(Writing, seconds dt);
        void presentCamera(Writing, rmmr::scene::Camera::Id);
        void setCameraKind(Writing, Cameras::Kind);
        void handleCameraHotkey(Writing);
        void engageInputs(Writing);
        void trackSpectator(Writing);
        auto focusCenter(Reading) const -> base::maybe<dvec3>;
        void drawInspectorWindow(Writing);
        void drawSpaceWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
        void drawAssemblerWindow(Writing);
        void openPlanetScenario(Writing);
        void closeLocalityScenario(Writing);
        void clearLocalityPopulation(Writing);
    };

}
