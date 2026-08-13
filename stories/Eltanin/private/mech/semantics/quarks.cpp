#include "mech/semantics/quarks.h"

#include <vector>

namespace eltanin::mech::skeleton {

    auto worldPose(const space::cell::Pose& cell, space::orient::key local) -> space::cell::Pose {
        const auto cellOri = static_cast<space::orient::key>(cell.ori);
        return space::cell::Pose{
            .pos = cell.pos,
            .ori = space::orient::compose[static_cast<std::size_t>(cellOri)][static_cast<std::size_t>(local)],
        };
    }

    auto seedCorners(frame::shape shape) -> std::vector<Corner> {
        const auto found = recipes.find(shape);
        if (found == recipes.end())
            return {};

        std::vector<Corner> out;
        out.reserve(found->second.corners.size());
        for (const auto& corner : found->second.corners)
            out.push_back(Corner{.kind = corner.kind, .ori = corner.orient});
        return out;
    }

    auto seedHalfribs(frame::shape shape) -> std::vector<Halfrib> {
        const auto found = recipes.find(shape);
        if (found == recipes.end())
            return {};

        std::vector<Halfrib> out;
        out.reserve(found->second.edges.size() * 2);
        for (const auto& edge : found->second.edges) {
            out.push_back(Halfrib{.kind = edge.kind, .pole = edge.poleAtMesh0, .ori = edge.orient});
            out.push_back(Halfrib{.kind = edge.kind, .pole = opposite(edge.poleAtMesh0), .ori = edge.orient});
        }
        return out;
    }

} // namespace eltanin::mech::skeleton
