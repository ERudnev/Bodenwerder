#pragma once

#include <map>
#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/mech/mount.q1.h>
#include <eltanin/mech/semantics.q1.h>

namespace eltanin::views::blueprints::mountEditor {

    // One admissible discrete orientation of an attachment about its BBox center.
    // p' = R·p + shift, with shift = (d − R·d)/2 and d = min+max (doubled center). Identity → shift 0.
    struct Spin {
        base::common_types::ivec3 shift;
        std::vector<base::common_types::index3> points; // native points after R about BBox center
        bool flip; // true → reverses plane normal; at most one such entry (escape hatch in ori menu)
    };

    // Oris that permute the attachment set onto itself. Flat: same-normal autos + one labeled flip.
    using Spins = std::map<mech::space::orient::key, Spin>;

    // Translation-invariant ordered point set (lex-min origin, then sort by xyz).
    using Ordered = std::vector<base::common_types::index3>;

    struct OrderedLess {
        auto operator()(const Ordered& a, const Ordered& b) const -> bool;
    };

    // Each of 24 rotations of the attachment → ordered shape → first ori that yields it.
    using Fits = std::map<Ordered, mech::space::orient::key, OrderedLess>;

    auto buildSpins(const mech::Attachment&) -> Spins;
    auto buildFits(const mech::Attachment&) -> Fits;
    auto matchesCursor(const Fits&, const std::vector<base::common_types::index3>& cursor) -> bool;
    auto seatingOn(const mech::Attachment&, const Fits&, const std::vector<base::common_types::index3>& cursor) -> base::maybe<mech::space::Transform>;

    // Body-local spin: R_new = compose(current, auto); grid += currentR · shift(auto). Empty if auto is identity.
    auto applyOri(const mech::space::Transform& current, const Spins&, mech::space::orient::key bodyAuto) -> base::maybe<mech::space::Transform>;

    // Near-cursor list: spins as body-local autos (identity = current). Full cube (24) → 6 turn/bank/tilt ±90°.
    auto drawOriMenu(mech::space::orient::key currentAbs, const Spins&) -> base::maybe<mech::space::orient::key>;

} // namespace eltanin::views::blueprints::mountEditor
