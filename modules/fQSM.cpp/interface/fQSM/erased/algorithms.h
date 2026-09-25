#pragma once

#include <string>
#include <string_view>

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace fqsm::erased {

    // Tombstones erase from target, other patchlets insert or overwrite; a changed global replaces target's.
    void integrate(Line& target, const PatchLine& patch);
    // The same, moving the values out of patch (for a patch that is discarded right after).
    void integrate_move(Line& target, PatchLine& patch);

    // Folds source into target as seen against base (clean delta): additions and updates become
    // modifications, removals become deletions; a changed global replaces target's.
    void merge_into(const ReadLine& base, PatchLine& target, const PatchLine& source);

    // "<name> [global, ??] [#id, del] [#id, ??]", or empty when patch has no changes.
    std::string format_patch_line(const PatchLine& patch, std::string_view name);
}
