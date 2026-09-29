#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/system/window.q1.h>
#include <rmmr/wrapper/library.h>
#include <rmmr/wrapper/product.h>

#include "blueprints/catalog.h"
#include "geo/celestial/planet.h"
#include "physics/system.h"
#include "physics/ui.h"
#include "resources/library.h"
#include "scenarios/planeliod.h"

#include <fQSM/api/interface.h>

namespace eltanin::views::locality {

    using namespace fqsm::api;

    struct Focus {
        vector<::eltanin::locality::Thing::Id> things;
    };

    struct View {
        struct Cameras {
            enum class Kind { free, spectator };
            Kind kind;
            rmmr::scene::Camera::Id free;
            rmmr::scene::Camera::Id spectator;
            rmmr::system::ViewInput::Id freeInput;
            rmmr::system::ViewInput::Id spectatorInput;
            bool hotkeyDown;
        };

        struct Panels {
            struct Inspector {};
            base::maybe<Inspector> inspector;

            struct Space {};
            base::maybe<Space> space;

            struct Lighting {};
            base::maybe<Lighting> lighting;

            struct Materials {
                using MaterialId = rmmr::resource::material::Asset::Id;
                struct NameEdit {
                    std::array<char, 256> buf;
                    bool editing;
                };
                base::maybe<MaterialId> selected;
                std::array<char, 128> filter;
                std::unordered_map<MaterialId, NameEdit> nameEdits;
            };
            base::maybe<Materials> materials;

            base::maybe<phys::Ui> physics;

            struct Assembler {
                base::maybe<mech::Blueprint::Id> blueprint;
                rmmr::Pos spawnPos;
                rmmr::HPB spawnHpb;
                rmmr::vec3 spawnVel;
                bool spawnAtCamera;
            };
            Assembler assembler;
        };

        base::maybe<rmmr::wrapper::Product::View> view;
        base::maybe<phys::System> physics;
        base::maybe<planet::Planet> planet;
        scenario::Planeliod planeliod;
        Focus focus;
        base::maybe<Cameras> cameras;
        Panels panels;

        auto create(Writing, rmmr::system::Window::Id, const assets::Handles&) -> bool;
        void tick(Stewarding, seconds dt);
        void handleCameraHotkey(Writing);
        void present(std::vector<rmmr::wrapper::Product::View>&) const;
        void engageCameras(Writing, bool shown);
        void contributeMenu(Writing, const assets::Handles&, const rmmr::wrapper::assets::Handles&);
        void draw(Writing, BlueprintCatalog&);

    private:
        void bindEntities(Writing);
        void presentCamera(Writing, rmmr::scene::Camera::Id);
        void setCameraKind(Writing, Cameras::Kind);
        void trackSpectator(Writing);
        auto focusCenter(Reading) const -> base::maybe<dvec3>;
        void drawInspectorWindow(Writing);
        void drawSpaceWindow(Writing);
        void drawLightingWindow(Writing);
        void drawMaterialsWindow(Writing);
        void drawMaterialInspector(Writing, rmmr::resource::material::Asset::Id);
        void drawAssemblerWindow(Writing, BlueprintCatalog&);
    };

}
