#pragma once

#include <cstddef>
#include <map>

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace tests::erased {

    inline fqsm::erased::PatchLine int_patch() {
        return fqsm::erased::PatchLine(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<int>());
    }

    inline void put(fqsm::erased::PatchLine& patch, fqsm::RawId id, int value, bool tombstone = false) {
        if (tombstone) patch.del(id, &value);
        else patch.modify(id, &value);
    }

    // Sequential application of patch onto a plain model.
    inline void apply_to(const fqsm::erased::PatchLine& patch, std::map<fqsm::RawId, int>& model) {
        for (std::size_t i = 0; i < patch.count(); ++i) {
            const auto patchlet = patch.at(i);
            if (patchlet.tombstone) model.erase(patch.id_at(i));
            else model[patch.id_at(i)] = *static_cast<const int*>(patchlet.value);
        }
    }

    inline std::map<fqsm::RawId, int> collect(const fqsm::erased::ReadLine& line) {
        std::map<fqsm::RawId, int> out;
        for (auto it = line.cursor_begin(), end = line.cursor_end(); not (it == end); ++it) {
            const auto entry = *it;
            out.emplace(entry.id, *static_cast<const int*>(entry.value));
        }
        return out;
    }

    inline std::size_t steps(const fqsm::erased::ReadLine& line) {
        std::size_t out = 0;
        for (auto it = line.cursor_begin(), end = line.cursor_end(); not (it == end); ++it)
            ++out;
        return out;
    }
}
