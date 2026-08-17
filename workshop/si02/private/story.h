#pragma once

#include <memory>

#include <rmmr/wrapper/product.h>

#include "assets.h"

namespace si02 {

    using namespace fqsm::api;

    class SpriteTest : public rmmr::wrapper::Product {
    public:
        struct Ui {
            bool camera = false;
            bool hud = true;
            bool ship = true;
            float zoom = 1.0f; // 1 = 1600×900 ortho; larger = zoomed out
        };

        std::unique_ptr<Assets> assets;
        Ui ui;

        Schema schema() const override;
        void addAssets(Writing context) override {
            assets = Assets::init(context);
        }
        void setup(Writing, rmmr::system::Window::Id) override;
        void onFrame(establish::Realm&, int64 dt_us) override;
        void contributeViewMenu(Writing) override;
        void drawUi(Writing) override;

    private:
        void drawHud(Writing);
        void drawShipPanel(Writing);
        void drawCameraWindow(Writing);
    };

}
