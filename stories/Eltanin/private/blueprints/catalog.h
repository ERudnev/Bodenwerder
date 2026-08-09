#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <base/maybe.h>
#include <eltanin/resources/blueprint.q1.h>
#include <fQSM/api/interface.h>

namespace eltanin {

    using namespace fqsm::api;

    // Game-owned blueprint pack on disk. Editor / Ships are consumers — not owners.
    struct BlueprintCatalog {
        filepath directory;
        std::vector<resource::blueprint::Asset::Id> items;
        std::array<char, 128> newName;
        bool ready;

        void bind(filepath); // directory + clear items / UI buf; marks not ready
        // One branch per file; skips broken. Idempotent after ready.
        void loadFromDisk(establish::Realm&);
        // One file in the current Writing; refuse on parse/IO → loadFromDisk isolates via branch.
        auto loadOne(Writing, string stem) -> base::maybe<resource::blueprint::Asset::Id>;
        // Empty ship + save; appends to items on success.
        auto createNew(Writing, std::string_view name) -> base::maybe<resource::blueprint::Asset::Id>;
    };

}
