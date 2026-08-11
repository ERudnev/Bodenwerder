#include "mech/semantics/quarks.h"

#include <vector>

namespace eltanin::mech::quarks {

    auto worldPose(const space::cell::Pose& cell, LocalOri local) -> space::cell::Pose {
        const auto cellOri = static_cast<space::orient::key>(cell.ori);
        const auto localOri = static_cast<space::orient::key>(local);
        return space::cell::Pose{
            .pos = cell.pos,
            .ori = static_cast<rmmr::renderer::Signed32>(space::orient::compose[static_cast<std::size_t>(cellOri)][static_cast<std::size_t>(localOri)]),
        };
    }

    auto seedCorners(frame::shape shape) -> std::vector<Knot> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        std::vector<Knot> out;
        out.reserve(found->second.corners.size());
        for (const auto& corner : found->second.corners)
            out.push_back(Knot{.kind = corner.kind, .ori = static_cast<LocalOri>(corner.orient)});
        return out;
    }

    auto seedHalfChords(frame::shape shape) -> std::vector<HalfChord> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        std::vector<HalfChord> out;
        out.reserve(found->second.edges.size() * 2);
        for (const auto& edge : found->second.edges) {
            const auto ori = static_cast<LocalOri>(edge.orient);
            out.push_back(HalfChord{.kind = edge.kind, .pole = edge.poleAtMesh0, .ori = ori});
            out.push_back(HalfChord{.kind = edge.kind, .pole = subframe::halfEdge::opposite(edge.poleAtMesh0), .ori = ori});
        }
        return out;
    }

} // namespace eltanin::mech::quarks
