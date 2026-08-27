#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <base/maybe.h>
#include <eltanin/resources/assets.q1.h>
#include <fQSM/api/interface.h>

namespace eltanin {

    using namespace fqsm::api;

    // Linear pack of blueprint asset ids (ships, prefabs, spawn menu, …).
    using BlueprintIds = std::vector<mech::Blueprint::Id>;

    enum class BlueprintShelf {
        ships,
        prefabs,
    };

    // Disk layout under assets/Eltanin/blueprints/:
    //   _unnamed.blueprint     — editor fallback (not a product)
    //   ships/*.blueprint      — products (editor + Assembler spawn)
    //   prefabs/*.blueprint    — editor building blocks + Assembler spawn
    struct BlueprintCatalog {
        filepath root;
        BlueprintIds ships;
        BlueprintIds prefabs;
        base::maybe<mech::Blueprint::Id> unnamed;
        std::array<char, 128> newName;
        bool ready;

        void bind(filepath); // root + clear packs / UI buf; marks not ready
        // One branch per file; skips broken. Ensures _unnamed. Idempotent after ready.
        void loadFromDisk(establish::Realm&);
        auto loadOne(Writing, BlueprintShelf, string stem) -> base::maybe<mech::Blueprint::Id>;
        auto ensureUnnamed(Writing) -> base::maybe<mech::Blueprint::Id>;
        // Empty blueprint + save into ships/ or prefabs/; appends to the matching pack.
        auto createNew(Writing, BlueprintShelf, std::string_view name) -> base::maybe<mech::Blueprint::Id>;
    };

}
