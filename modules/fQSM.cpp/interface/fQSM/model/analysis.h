#pragma once

#include <unordered_map>

#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>

namespace fqsm::analysis {

    struct Patch {
        struct Overall {
            int nonEmptyLines{};
            int patchlets{};
        };
        struct SliceEntry {
            int deleted = 0;
            int modified = 0;

            int total() const { return deleted + modified; }
        };
        std::unordered_map<meta::Rtid, SliceEntry, meta::Rtid::Hash> perSlice;
        Overall overall;

        explicit Patch(const model::complex::Patch&);
    };
}