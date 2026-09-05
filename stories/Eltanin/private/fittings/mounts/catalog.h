#pragma once

#include <filesystem>
#include <vector>

#include <base/maybe.h>
#include <eltanin/mech/mount.q1.h>
#include <fQSM/api/interface.h>

namespace eltanin {

    using namespace fqsm::api;

    using MountIds = std::vector<mech::Mount::Id>;

    // Disk: assets/Eltanin/fittings/<shelf>/*.json → Eltanin::<shelf>.<stem>
    struct MountCatalog {
        filepath root;
        MountIds ids;
        bool ready;

        void bind(filepath);
        void loadFromDisk(establish::Realm&);
        auto loadOne(Writing, string shelf, string stem) -> base::maybe<mech::Mount::Id>;
    };

}
