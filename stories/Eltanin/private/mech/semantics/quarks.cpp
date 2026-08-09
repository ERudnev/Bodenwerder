#include "mech/semantics/quarks.h"

#include <algorithm>
#include <vector>

namespace eltanin::mech::quarks {

    namespace {

        using space::orient::key;

        auto placementLess(const space::cell::Placement& a, const space::cell::Placement& b) -> bool {
            if (a.cell.x != b.cell.x) return a.cell.x < b.cell.x;
            if (a.cell.y != b.cell.y) return a.cell.y < b.cell.y;
            if (a.cell.z != b.cell.z) return a.cell.z < b.cell.z;
            return a.corner < b.corner;
        }

        auto placementSame(const space::cell::Placement& a, const space::cell::Placement& b) -> bool {
            return a.cell.x == b.cell.x and a.cell.y == b.cell.y and a.cell.z == b.cell.z and a.corner == b.corner;
        }

        // Round-trip check: seeding this placement under recovered cellOri must land on node with worldOri.
        auto matchesSeed(const space::cell::Placement& placement, key cellOri, key recipeOrient, const space::grid::point& node, key worldOri) -> bool {
            if (space::orient::compose[static_cast<std::size_t>(cellOri)][static_cast<std::size_t>(recipeOrient)] != worldOri)
                return false;
            const auto aabbCorner = space::orient::cornerIndex(cellOri, placement.corner);
            const auto& delta = cube::corners[static_cast<std::size_t>(aabbCorner)];
            return placement.cell.x + delta.x == node.x
                and placement.cell.y + delta.y == node.y
                and placement.cell.z + delta.z == node.z;
        }

    } // namespace

    auto Knot::evaluateCellPlacement() const -> space::cell::Placement {
        // A grid node sits on up to 8 cells. Kind + world ori recover the cell-local recipe
        // seat (cellVertex, recipe orient) via: worldOri = cellOri ∘ recipeOrient
        // ⇒ cellOri = worldOri ∘ invert(recipeOrient). Then AABB corner + node pin cell index.
        // Several recipe rows can yield distinct Placements; we keep verified unique hits and
        // pick lexicographic minimum for a stable collision key. No recipe match → owner cell
        // whose min-corner is this node (corner 0) — hand-placed grid knot convention.

        const space::grid::point node{pose.pos.x, pose.pos.y, pose.pos.z};
        const auto worldOri = static_cast<key>(pose.ori);

        std::vector<space::cell::Placement> hits;
        hits.reserve(8);

        for (const auto& [shape, recipe] : subframe::recipes) {
            (void)shape;
            for (const auto& row : recipe.corners) {
                if (row.kind != kind)
                    continue;
                const auto cellOri = space::orient::compose[static_cast<std::size_t>(worldOri)][static_cast<std::size_t>(space::orient::invert[static_cast<std::size_t>(row.orient)])];
                const auto aabbCorner = space::orient::cornerIndex(cellOri, row.cellVertex);
                const auto& delta = cube::corners[static_cast<std::size_t>(aabbCorner)];
                const space::cell::Placement candidate{
                    .cell = space::cell::index{
                        node.x - delta.x,
                        node.y - delta.y,
                        node.z - delta.z,
                    },
                    .corner = row.cellVertex,
                };
                if (not matchesSeed(candidate, cellOri, row.orient, node, worldOri))
                    continue;
                hits.push_back(candidate);
            }
        }

        if (hits.empty()) {
            return space::cell::Placement{
                .cell = space::cell::index{node.x, node.y, node.z},
                .corner = 0,
            };
        }

        std::sort(hits.begin(), hits.end(), placementLess);
        hits.erase(std::unique(hits.begin(), hits.end(), placementSame), hits.end());
        return hits.front();
    }

    auto seedCorners(frame::shape shape, space::cell::Pose cell) -> std::vector<Knot> {
        const auto found = subframe::recipes.find(shape);
        if (found == subframe::recipes.end())
            return {};

        const auto cellOri = static_cast<key>(cell.ori);
        std::vector<Knot> out;
        out.reserve(found->second.corners.size());

        for (const auto& corner : found->second.corners) {
            const auto worldCorner = space::orient::cornerIndex(cellOri, corner.cellVertex);
            const auto& local = cube::corners[static_cast<std::size_t>(worldCorner)];
            out.push_back(Knot{
                .kind = corner.kind,
                .pose = space::grid::Pose{
                    .pos = base::common_types::index3{
                        .x = cell.pos.x + local.x,
                        .y = cell.pos.y + local.y,
                        .z = cell.pos.z + local.z,
                    },
                    .ori = static_cast<rmmr::renderer::Signed32>(space::orient::compose[static_cast<std::size_t>(cellOri)][static_cast<std::size_t>(corner.orient)]),
                },
            });
        }
        return out;
    }

} // namespace eltanin::mech::quarks
