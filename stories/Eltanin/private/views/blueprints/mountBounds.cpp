#include "views/blueprints/mountBounds.h"

#include "mech/semantics/space.h"

#include <algorithm>
#include <cstdint>

namespace eltanin::views::blueprints::mountBounds {

    namespace {

        auto rotateLocal(mech::space::orient::key rotation, base::common_types::index3 local) -> base::common_types::index3 {
            const auto& matrix = mech::space::orient::matrix[static_cast<std::size_t>(rotation)];
            const auto rotated = matrix * mech::space::ivec3{local.x, local.y, local.z};
            return base::common_types::index3{.x = rotated.x, .y = rotated.y, .z = rotated.z};
        }

        auto worldPoint(const mech::space::Transform& transform, base::common_types::index3 local) -> base::common_types::index3 {
            const auto rotated = rotateLocal(transform.rotation, local);
            return base::common_types::index3{.x = transform.grid.x + rotated.x, .y = transform.grid.y + rotated.y, .z = transform.grid.z + rotated.z};
        }

        auto axisCellRange(int gmin, int gmax, bool flatAxis) -> std::pair<int, int> {
            if (flatAxis or gmin == gmax)
                return {gmin, gmin};
            return {gmin, gmax - 1};
        }

    } // namespace

    void include(CellBox& box, base::common_types::index3 cell) {
        box.min.x = std::min(box.min.x, cell.x);
        box.min.y = std::min(box.min.y, cell.y);
        box.min.z = std::min(box.min.z, cell.z);
        box.max.x = std::max(box.max.x, cell.x);
        box.max.y = std::max(box.max.y, cell.y);
        box.max.z = std::max(box.max.z, cell.z);
    }

    void include(CellBox& box, const CellBox& other) {
        include(box, other.min);
        include(box, other.max);
    }

    auto cellBox(const mech::Attachment& attachment, const mech::space::Transform& transform) -> base::maybe<CellBox> {
        if (attachment.points.empty())
            return {};

        auto gmin = worldPoint(transform, attachment.points.front());
        auto gmax = gmin;
        for (std::size_t i = 1; i < attachment.points.size(); ++i) {
            const auto world = worldPoint(transform, attachment.points[i]);
            gmin.x = std::min(gmin.x, world.x);
            gmin.y = std::min(gmin.y, world.y);
            gmin.z = std::min(gmin.z, world.z);
            gmax.x = std::max(gmax.x, world.x);
            gmax.y = std::max(gmax.y, world.y);
            gmax.z = std::max(gmax.z, world.z);
        }

        const bool flat = attachment.flatMounted();
        const auto [x0, x1] = axisCellRange(gmin.x, gmax.x, flat and gmin.x == gmax.x);
        const auto [y0, y1] = axisCellRange(gmin.y, gmax.y, flat and gmin.y == gmax.y);
        const auto [z0, z1] = axisCellRange(gmin.z, gmax.z, flat and gmin.z == gmax.z);

        // Non-axis-aligned flat (skew plane): fall back to volume rule on the grid AABB.
        if (flat and gmin.x != gmax.x and gmin.y != gmax.y and gmin.z != gmax.z) {
            return CellBox{
                .min = {.x = gmin.x, .y = gmin.y, .z = gmin.z},
                .max = {.x = std::max(gmin.x, gmax.x - 1), .y = std::max(gmin.y, gmax.y - 1), .z = std::max(gmin.z, gmax.z - 1)},
            };
        }

        return CellBox{.min = {.x = x0, .y = y0, .z = z0}, .max = {.x = x1, .y = y1, .z = z1}};
    }

} // namespace eltanin::views::blueprints::mountBounds
