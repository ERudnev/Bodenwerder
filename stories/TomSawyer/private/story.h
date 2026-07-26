#pragma once

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/wrapper/product.h>

namespace tommy {

    using namespace fqsm::api;

    class SpriteTest : public rmmr::wrapper::Product {
    public:
        struct Assets {
            base::maybe<rmmr::resource::sprite::Pack::Id> kenney;
            base::maybe<rmmr::resource::geometry::Asset::Id> unitQuad;
        };

        struct Ui {
            bool camera = true;
        };

        Assets assets;
        Ui ui;

        Schema schema() const override;
        void addAssets(Writing, rmmr::system::Core::Id) override;
        void setup(Writing, rmmr::system::Core::Id, rmmr::system::Viewport::Id) override;
        void contributeViewMenu() override;
        void drawUi(Writing) override;

    private:
        void drawCameraWindow(Writing);
    };

}
