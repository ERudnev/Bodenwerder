#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>

#include "demo.h"

namespace toy::demos {

    class KubeOfKubes : public Demo {
    public:
        struct Assets {
            struct {
                base::maybe<rmmr::resource::geometry::Asset::Id> triangle;
                base::maybe<rmmr::resource::geometry::Asset::Id> kube;
                base::maybe<rmmr::resource::geometry::Asset::Id> bagel;
                base::maybe<rmmr::resource::geometry::Asset::Id> grid;
            } primitive;
        };

        struct Ui {
            bool camera = true;
            bool lighting = true;
            bool materials = true;

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

        void seedAssets(Writing, rmmr::system::Core::Id, const assets::Handles&) override;
        Handles setup(Writing, const assets::Handles&) override;
        void contributeViewMenu() override;
        void drawUi(Writing, const Handles&) override;

    private:
        void drawCameraWindow(Writing, const Handles&);
        void drawLightingWindow(Writing, const Handles&);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
    };

}
