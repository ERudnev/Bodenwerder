#include "mech/semantics/quarks.h"

#include <vector>

namespace eltanin::mech::quarks {

    namespace {

        using space::orient::key;

        auto gridPoseAtCellCorner(key cellOri, const space::cell::Pose& cell, cube::Corner cellVertex, key recipeOrient) -> space::grid::Pose {
            const auto worldCorner = space::orient::cornerIndex(cellOri, cellVertex);
            const auto& local = cube::corners[static_cast<std::size_t>(worldCorner)];
            return space::grid::Pose{
                .pos = base::common_types::index3{
                    .x = cell.pos.x + local.x,
                    .y = cell.pos.y + local.y,
                    .z = cell.pos.z + local.z,
                },
                .ori = static_cast<rmmr::renderer::Signed32>(space::orient::compose[static_cast<std::size_t>(cellOri)][static_cast<std::size_t>(recipeOrient)]),
            };
        }

    } // namespace

    auto seedCorners(frame::shape shape, space::cell::Pose cell) -> std::vector<Knot> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        const auto cellOri = static_cast<key>(cell.ori);
        std::vector<Knot> out;
        out.reserve(found->second.corners.size());

        for (const auto& corner : found->second.corners) {
            out.push_back(Knot{
                .kind = corner.kind,
                .pose = gridPoseAtCellCorner(cellOri, cell, corner.cellVertex, corner.orient),
            });
        }
        return out;
    }

    auto seedChords(frame::shape shape, space::cell::Pose cell) -> std::vector<Chord> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        const auto cellOri = static_cast<key>(cell.ori);
        std::vector<Chord> out;
        out.reserve(found->second.edges.size());

        for (const auto& edge : found->second.edges) {
            out.push_back(Chord{
                .kind = edge.kind,
                .pole = edge.poleAtMesh0,
                .pose = gridPoseAtCellCorner(cellOri, cell, edge.cellAt0, edge.orient),
            });
        }
        return out;
    }

} // namespace eltanin::mech::quarks
