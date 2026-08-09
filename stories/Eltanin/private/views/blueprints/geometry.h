#pragma once

#include <cstdint>
#include <vector>

#include <base/maybe.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include "mech/blueprint.h"
#include "mech/semantics/quarks.h"
#include "mech/semantics/space.h"

#include <fQSM/api/interface.h>

namespace eltanin::views::blueprints::geometry {

    using namespace fqsm::api;

    struct QuarkActor {
        enum class Kind : std::uint8_t { knot, halfChord };

        rmmr::scene::actor::Mesh::Id id;
        Kind kind;
        std::size_t index; // into Blueprint::knots / halfChords
    };

    // Home-cube pivot (±2 m) → cube corner index in the mesh's local frame.
    auto localSeatFromOrigin(rmmr::Pos origin) -> mech::cube::Corner;

    // Continuous actor pose: mesh local 0 at the cell corner selected by ori + Entry.origin.
    auto actorPose(const mech::space::cell::Pose& quarkPose, rmmr::Pos entryOrigin) -> rmmr::Pose;

    auto resolveKnot(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::quarks::Knot::Kind kind) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;
    auto resolveHalfChord(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::quarks::HalfChord::Kind kind, mech::subframe::halfEdge::Pole pole) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;

    void clearActors(Writing, rmmr::scene::Root::Id root, std::vector<QuarkActor>& actors);

    // Rebuild quark mesh actors for a blueprint (interframe pack). Each actor is Identified for pick/selection.
    void syncActors(Writing, rmmr::scene::Root::Id root, rmmr::resource::meshpack::Asset::Id interframe, const mech::Blueprint& blueprint, std::vector<QuarkActor>& actors);

} // namespace eltanin::views::blueprints::geometry
