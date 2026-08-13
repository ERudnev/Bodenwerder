#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <base/maybe.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/mount.q1.h>
#include <eltanin/mech/semantics.q1.h>
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
        int cellY;         // placement.cell.y at spawn — spatial filters
    };

    // Editor visibility filter — Layer flags + optional spatial floor.
    struct Display {
        bool skeleton;
        bool membranes;
        bool internals;
        bool externals;
        // When set: keep actors with cellY >= *cellYMin (groundwork for floor clip).
        base::maybe<int> cellYMin;

        auto shows(mech::Layer layer) const -> bool {
            switch (layer) {
                case mech::Layer::skeleton: return skeleton;
                case mech::Layer::membranes: return membranes;
                case mech::Layer::internals: return internals;
                case mech::Layer::externals: return externals;
            }
            return false;
        }

        auto spatialOk(int cellY) const -> bool {
            return not cellYMin.exists() or cellY >= *cellYMin;
        }

        auto showsQuark(QuarkActor::Kind kind, int cellY) const -> bool {
            if (not spatialOk(cellY))
                return false;
            switch (kind) {
                case QuarkActor::Kind::knot:
                case QuarkActor::Kind::halfChord: return skeleton;
                case QuarkActor::Kind::wall: return membranes;
            }
            return false;
        }

        auto showsMount(mech::Layer layer, int cellY) const -> bool {
            return spatialOk(cellY) and shows(layer);
        }
    };

    struct MountActor {
        rmmr::scene::actor::Mesh::Id id;
        std::size_t index; // into Blueprint::mounts
        mech::Layer layer;
        int cellY; // transform.grid.y at spawn
    };

    // Floor layout of library mounts (palette scene).
    struct PaletteMountActor {
        rmmr::scene::actor::Mesh::Id id;
        mech::Mount::Id mount;
    };

    // Home-cube pivot (±2 m) → cube corner index in the mesh's local frame.
    auto localSeatFromOrigin(rmmr::Pos origin) -> mech::cube::Corner;

    // Continuous actor pose for skeleton quarks: mesh local 0 at the cell corner selected by ori + Entry.origin.
    auto actorPose(const mech::space::cell::Placement& quarkPose, rmmr::Pos entryOrigin) -> rmmr::Pose;

    // Mount actor pose: LW contract — mesh pivot at attachment; transform.grid is that lattice point (meters = grid * edge), rotation = orient key.
    auto gridActorPose(const mech::space::Transform& transform) -> rmmr::Pose;

    auto resolveKnot(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::skeleton::Corner::Kind) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;
    auto resolveHalfChord(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::skeleton::Halfrib::Kind, mech::skeleton::Halfrib::Pole) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;
    auto resolveWall(Reading, rmmr::resource::meshpack::Asset::Id pack, mech::skeleton::Membrane::Kind) -> base::maybe<rmmr::resource::meshpack::Asset::Resolved>;

    void clearActors(Writing, rmmr::scene::Root::Id root, std::vector<QuarkActor>& actors);
    void clearMountActors(Writing, rmmr::scene::Root::Id root, std::vector<MountActor>& actors);
    void clearPaletteActors(Writing, rmmr::scene::Root::Id root, std::vector<PaletteMountActor>& actors);

    // Filter pass only — setVisible; actors stay resident (Layer + spatial).
    void applyDisplay(Writing, Display, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts);

    // Structural rebuild: spawn every quark/mount, then applyDisplay. Use on load / paste / undo / heavy edits.
    void syncActors(Writing, rmmr::scene::Root::Id root, rmmr::resource::meshpack::Asset::Id interframe, const Blueprint& blueprint, Display, std::vector<QuarkActor>& actors);
    void syncMountActors(Writing, rmmr::scene::Root::Id root, const Blueprint& blueprint, Display, const std::unordered_map<mech::Mount::Id, mech::Layer>& mountLayers, std::vector<MountActor>& actors);

    // Incremental membrane: one Identified wall actor; visibility from Display.
    auto appendWallActor(Writing, rmmr::scene::Root::Id root, rmmr::resource::meshpack::Asset::Id interframe, std::size_t cellIndex, std::size_t membraneIndex, const mech::space::cell::Placement& placement, const mech::skeleton::Membrane& wall, Display, std::vector<QuarkActor>& actors) -> bool;
    // Destroy wall actor at slot; fix membrane indices in the same cell.
    void eraseWallActor(Writing, rmmr::scene::Root::Id root, std::size_t actorSlot, std::vector<QuarkActor>& actors);

    // Floor grid of all catalog mounts (identity rotation; spacing in lattice cells).
    void syncPaletteActors(Writing, rmmr::scene::Root::Id root, const std::vector<mech::Mount::Id>& mounts, std::vector<PaletteMountActor>& actors);

    // Preview actors for clipboard paste: no Identified; additive ghost material + MeshState tint.
    void syncGhostActors(Writing, rmmr::scene::Root::Id root, rmmr::resource::meshpack::Asset::Id interframe, ::rmmr::resource::material::Asset::Id ghostMaterial, const Blueprint& blueprint, Display, std::vector<QuarkActor>& actors, rmmr::RGB albedo, float opacity);

    // In-place pose + MeshState update. False → caller must syncGhostActors.
    auto refreshGhostActors(Writing, rmmr::resource::meshpack::Asset::Id interframe, const Blueprint& blueprint, Display, std::vector<QuarkActor>& actors, rmmr::RGB albedo, float opacity) -> bool;

    } // namespace geometry

} // namespace eltanin::views::blueprints
