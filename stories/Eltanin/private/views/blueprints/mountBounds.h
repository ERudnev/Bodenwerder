#pragma once

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/mech/mount.q1.h>
#include <eltanin/mech/semantics.q1.h>

namespace eltanin::views::blueprints::mountBounds {

    // Inclusive AABB in cell-space (not grid). Editor-only seating heuristic — expect iteration.
    struct CellBox {
        base::common_types::index3 min;
        base::common_types::index3 max;
    };

    void include(CellBox& box, base::common_types::index3 cell);
    void include(CellBox& box, const CellBox& other);

    // Attachment points → world grid via Transform, then cell AABB.
    // Flat (coplanar): degenerate axis uses positive-side cell index (= grid value on that axis).
    //   e.g. p1111 on top of cell (0,0,0) → shared grid y=1 → cell (0,1,0).
    // Volume: cells covered by half-open grid interval [gmin, gmax).
    auto cellBox(const mech::Attachment& attachment, const mech::space::Transform& transform) -> base::maybe<CellBox>;

} // namespace eltanin::views::blueprints::mountBounds
