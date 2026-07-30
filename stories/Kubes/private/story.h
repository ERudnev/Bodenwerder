#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/wrapper/product.h>

namespace kubes {

    using namespace fqsm::api;

    class KubeOfKubes : public rmmr::wrapper::Product {
    public:
        struct Assets {
            struct {
                base::maybe<rmmr::resource::geometry::Asset::Id> grid;
            } primitive;
        };

        struct Ui {
            bool camera = false;
            bool lighting = false;
            bool materials = false;

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

        Schema schema() const override;
        void addAssets(Writing, rmmr::system::Core::Id) override;
        void setup(establish::Realm&, rmmr::system::Core::Id, rmmr::system::Window::Id) override;
        void contributeViewMenu() override;
        void drawUi(Writing) override;

    private:
        void populateWorld(Writing, rmmr::system::Window::Id);
        void drawCameraWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
    };

}
