#pragma once

#include <unordered_map>

#include <fQSM/meta/runtimeId.h>
#include <fQSM/state/_forwards.h>

namespace fqsm::analysis {

    struct Patch {
        struct SliceEntry {
            int deleted = 0;
            int modified = 0;

            int total() const { return deleted + modified; }
        };

        std::unordered_map<meta::aspect::Rtid, SliceEntry, meta::aspect::Rtid::Hash> perSlice;

        explicit Patch(const state::world::Patch&);

        int overallChanges() const;
    };
}