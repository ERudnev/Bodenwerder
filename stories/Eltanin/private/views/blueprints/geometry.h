#pragma once

#include <cstdint>
#include <vector>

#include <base/maybe.h>
#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include "mech/semantics/quarks.h"
#include "mech/semantics/space.h"

#include <fQSM/api/interface.h>

namespace eltanin::views::blueprints {

    using Blueprint = mech::Blueprint::Quantum;
    using Cell = mech::Blueprint::Cell;

    namespace geometry {

    using namespace fqsm::api;

    struct QuarkActor {
        enum class Kind : std::uint8_t { knot, halfChord, wall };

        rmmr::scene::actor::Mesh::Id id;
        Kind kind;
        std::size_t cell;  // into Blueprint::cells
        std::size_t index; // into cell.corners / halfribs / membranes
    };

    // Editor visibility filter for quark actors (skeleton = frame knots/half-chords).
    struct Display {
        bool skeleton;
        bool hull;
    };

    // Home-cube pivot (±2 m) → cube corner index in the mesh's local frame.
    auto localSeatFromOrigin(rmmr::Pos origin) -> mech::cube::Corner;

    // Continuous actor pose: mesh local 0 at the cell corner selected by ori + Entry.origin.
    auto actorPose(const mech::space::cell::Pose& quarkPose, rmmr::Pos entryOrigin) -> rmmr::Pose;

    auto resolveKnot(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::skeleton::Corner::Kind) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;
    auto resolveHalfChord(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::skeleton::Halfrib::Kind, mech::skeleton::Halfrib::Pole) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;
    auto resolveWall(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::skeleton::Membrane::Kind) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;

    void clearActors(Writing, rmmr::scene::Root::Id root, std::vector<QuarkActor>& actors);

    // Rebuild quark mesh actors for a blueprint (interframe pack). Each actor is Identified for pick/selection.
    void syncActors(Writing, rmmr::scene::Root::Id root, rmmr::resource::meshpack::Asset::Id interframe, const Blueprint& blueprint, Display, std::vector<QuarkActor>& actors);

    // Preview actors for clipboard paste: no Identified; additive ghost material + MeshState tint.
    void syncGhostActors(Writing, rmmr::scene::Root::Id root, rmmr::resource::meshpack::Asset::Id interframe, ::rmmr::resource::material::Asset::Id ghostMaterial, const Blueprint& blueprint, Display, std::vector<QuarkActor>& actors, rmmr::RGB albedo, float opacity);

    // In-place pose + MeshState update. False → caller must syncGhostActors.
    auto refreshGhostActors(Writing, rmmr::resource::meshpack::Asset::Id interframe, const Blueprint& blueprint, Display, std::vector<QuarkActor>& actors, rmmr::RGB albedo, float opacity) -> bool;

    } // namespace geometry

} // namespace eltanin::views::blueprints
