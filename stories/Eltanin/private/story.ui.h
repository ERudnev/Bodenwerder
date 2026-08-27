#pragma once

#include <array>
#include <unordered_map>

#include <base/maybe.h>
#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/materials.q1.h>

#include "physics/ui.h"

namespace eltanin {

    // Panel open ≡ maybe has_value(); nested fields = that window's UI state.
    struct Ui {
        struct Camera {};
        base::maybe<Camera> camera;

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

        struct Blueprints {};
        base::maybe<Blueprints> blueprints;

        // Always-on spawn panel (no view-menu toggle).
        struct Assembler {
            base::maybe<mech::Blueprint::Id> prefab;
            rmmr::Pos spawnPos;
            rmmr::HPB spawnHpb;
        };
        Assembler assembler{};
    };

}
