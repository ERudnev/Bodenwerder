#pragma once

#include "demo.h"

namespace toy::demos {

    class SpriteTest : public Demo {
    public:
        // Demo-local catalogue ids (shared toy::assets::Handles stay on Application).
        struct Assets {
        };

        Assets assets;

        void seedAssets(Writing, rmmr::system::Core::Id, const assets::Handles&) override;
        Handles setup(Writing, const assets::Handles&) override;
    };

}
