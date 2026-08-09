#include "mech/semantics/quarks.h"

#include <vector>

namespace eltanin::mech::quarks {

    namespace {

        using space::orient::key;

        auto cellPose(const space::cell::Pose& cell, key recipeOrient) -> space::cell::Pose {
            const auto cellOri = static_cast<key>(cell.ori);
            return space::cell::Pose{
                .pos = cell.pos,
                .ori = static_cast<rmmr::renderer::Signed32>(space::orient::compose[static_cast<std::size_t>(cellOri)][static_cast<std::size_t>(recipeOrient)]),
            };
        }

    } // namespace

    auto seedCorners(frame::shape shape, space::cell::Pose cell) -> std::vector<Knot> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        std::vector<Knot> out;
        out.reserve(found->second.corners.size());

        for (const auto& corner : found->second.corners) {
            out.push_back(Knot{
                .kind = corner.kind,
                .pose = cellPose(cell, corner.orient),
            });
        }
        return out;
    }

    auto seedHalfChords(frame::shape shape, space::cell::Pose cell) -> std::vector<HalfChord> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        std::vector<HalfChord> out;
        out.reserve(found->second.edges.size() * 2);

        for (const auto& edge : found->second.edges) {
            const auto pose = cellPose(cell, edge.orient);
            out.push_back(HalfChord{.kind = edge.kind, .pole = edge.poleAtMesh0, .pose = pose});
            out.push_back(HalfChord{.kind = edge.kind, .pole = subframe::halfEdge::opposite(edge.poleAtMesh0), .pose = pose});
        }
        return out;
    }

} // namespace eltanin::mech::quarks
