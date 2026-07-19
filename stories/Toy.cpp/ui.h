#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>

namespace toy::ui {

    using namespace fqsm::api;

    struct State {
        struct {
            bool camera = true;
            bool lighting = true;
            bool materials = true;
            bool stats = true;
        } views;

        base::maybe<rmmr::scene::Root::Id> scene;
        base::maybe<rmmr::scene::Camera::Id> camera;
        base::maybe<rmmr::resource::material::Asset::Id> selected_material;
        std::array<char, 128> material_filter{};

        struct MaterialNameEdit {
            std::array<char, 256> buf{};
            bool editing = false;
        };
        std::unordered_map<std::uint64_t, MaterialNameEdit> material_name_edits;

        void draw(Writing);

    private:
        void drawMainMenuBar();
        void drawCameraWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawStatsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
    };

}
