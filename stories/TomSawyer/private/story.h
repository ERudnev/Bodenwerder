#pragma once

#include <memory>

#include <rmmr/wrapper/product.h>

#include "assets.h"

namespace tommy {

    using namespace fqsm::api;

    class SpriteTest : public rmmr::wrapper::Product {
    public:
        struct Ui {
            bool camera = false;
            bool hud = true;
            bool gun = true;
        };

        std::unique_ptr<Assets> assets;
        Ui ui;

        Schema schema() const override;
        void addAssets(Writing context, rmmr::system::Core::Id id) override {
            assets = Assets::init(context, id);
        }
        void setup(Writing, rmmr::system::Core::Id, rmmr::system::Viewport::Id) override;
        void contributeViewMenu() override;
        void drawUi(Writing) override;

    private:
        void drawHud(Writing);
        void drawGunPanel(Writing);
        void drawCameraWindow(Writing);
    };

}
