#pragma once

#include <span>

#include <base/maybe.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"
#include "fittings/mounts/catalog.h"
#include "resources/library.h"
#include "views/blueprints/editor.h"
#include "views/locality/view.h"
#include "views/starMap/view.h"

namespace eltanin {

    using namespace fqsm::api;

    class Game : public rmmr::wrapper::Product {
    public:
        enum class Shown {
            map,
            locality,
            editor,
        };

        struct Menu {};

        ::eltanin::assets::Handles assets;
        base::maybe<Menu> menu;
        Shown shown;
        views::starmap::View starMap;
        base::maybe<views::locality::View> locality;
        views::Blueprints editor;
        BlueprintCatalog blueprintPack;
        MountCatalog mountPack;

        Schema schema() const override;
        void createCore(Writing) override;
        void addAssets(Writing) override;
        void prepareAssets(Writing) override;
        void setup(Writing, rmmr::system::Window::Id) override;
        void onFrame(establish::Realm&, int64 dt_us) override;
        void contributeViewMenu(Writing) override;
        void drawUi(Writing) override;
        void contributeEditorPanels(bool catalog);
        void contributeMenuBar(Writing);
        void drawMenuWindow(Writing);
        auto activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> override;
        auto overlaySelection() const -> std::span<const rmmr::renderer::Integer32> override;

    private:
        void engageInputs(Writing);
        void showMap(Writing);
        void showLocality(Writing);
        void showEditor(Writing);
    };

}
