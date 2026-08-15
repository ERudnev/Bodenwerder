#include "views/blueprints/mountPlacement.h"

#include "mech/semantics/shapes.h"
#include "mech/semantics/space.h"

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <base/logging.h>

#include <cmath>
#include <limits>

#include <glm/geometric.hpp>

namespace eltanin::views::blueprints::mountPlacement {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        constexpr float ballOpacity = 0.55f;
        constexpr float ballScale = 0.35f;
        const RGB ballAlbedo{1.0f, 0.72f, 0.22f};

        auto destroyBall(Writing context, scene::Root::Id root, scene::actor::Mesh::Id actor) -> void {
            // Group delete already removes the node; a second Node::remove is modify-after-delete → bad_optional_access.
            if (with<scene::Node_group>::exists(context, root) and with<scene::Node_group>::get(context, root).contains(actor)) {
                with<scene::Node_group>::deleteElement(context, root, actor);
                return;
            }
            for (const auto [otherRoot, group] : context->aspect<scene::Node_group>().items()) {
                if (group.contains(actor)) {
                    with<scene::Node_group>::deleteElement(context, otherRoot, actor);
                    return;
                }
            }
            if (with<scene::Node>::exists(context, actor))
                with<scene::Node>::remove(context, actor);
        }

        auto cornerWorld(const mech::space::cell::Placement& pose, mech::cube::Corner corner) -> Pos {
            const auto center = mech::space::cell::center2local(mech::space::cell::index{pose.cell.x, pose.cell.y, pose.cell.z});
            const auto local = mech::space::orient::cell2local(static_cast<mech::space::orient::key>(pose.ori), mech::cube::corners[static_cast<std::size_t>(corner)]);
            return Pos{center.x + local.x, center.y + local.y, center.z + local.z};
        }

        auto faceWorldLoop(const Cell& cell, mech::frame::FaceIndex face) -> std::vector<Pos> {
            const auto shapeIndex = static_cast<std::size_t>(cell.shape);
            const auto faceIndex = static_cast<std::size_t>(face);
            if (shapeIndex >= mech::frame::faces.size() or faceIndex >= mech::frame::faces[shapeIndex].size())
                return {};
            const auto& loop = mech::frame::faces[shapeIndex][faceIndex];
            std::vector<Pos> out;
            out.reserve(loop.size());
            for (const auto corner : loop)
                out.push_back(cornerWorld(cell.placement, corner));
            return out;
        }

        auto rayHitTriangle(const Pos& origin, const vec3& dir, const Pos& a, const Pos& b, const Pos& c, float& tOut) -> bool {
            constexpr float eps = 1e-6f;
            const vec3 ab = b - a;
            const vec3 ac = c - a;
            const vec3 pvec = glm::cross(dir, ac);
            const float det = glm::dot(ab, pvec);
            if (std::abs(det) < eps)
                return false;
            const float invDet = 1.0f / det;
            const vec3 tvec = origin - a;
            const float u = glm::dot(tvec, pvec) * invDet;
            if (u < 0.0f or u > 1.0f)
                return false;
            const vec3 qvec = glm::cross(tvec, ab);
            const float v = glm::dot(dir, qvec) * invDet;
            if (v < 0.0f or u + v > 1.0f)
                return false;
            const float t = glm::dot(ac, qvec) * invDet;
            if (t < eps)
                return false;
            tOut = t;
            return true;
        }

        auto rayHitPolygon(const Pos& origin, const vec3& dir, const std::vector<Pos>& loop, float& tOut) -> bool {
            if (loop.size() < 3)
                return false;
            bool hit = false;
            float best = tOut;
            for (std::size_t i = 1; i + 1 < loop.size(); ++i) {
                float t = 0.0f;
                if (rayHitTriangle(origin, dir, loop[0], loop[i], loop[i + 1], t) and (not hit or t < best)) {
                    best = t;
                    hit = true;
                }
            }
            if (hit)
                tOut = best;
            return hit;
        }

        auto gridMeters(const base::common_types::index3& grid) -> Pos {
            const float edge = mech::space::local::edge2meters;
            return Pos{static_cast<float>(grid.x) * edge, static_cast<float>(grid.y) * edge, static_cast<float>(grid.z) * edge};
        }

        auto length2(const vec3& v) -> float {
            return glm::dot(v, v);
        }

        // Four virtual p121 on a square loop (omit one corner each); pick nearest ray hit (centroid tiebreak).
        auto nearestSquareTriPoints(const std::vector<base::common_types::index3>& quad, const std::vector<Pos>& worldLoop, const MouseRay& ray) -> std::vector<base::common_types::index3> {
            if (quad.size() != 4 or worldLoop.size() != 4)
                return quad;
            float bestT = std::numeric_limits<float>::infinity();
            float bestCentroidDist2 = std::numeric_limits<float>::infinity();
            int best = -1;
            for (int i = 0; i < 4; ++i) {
                const Pos& a = worldLoop[static_cast<std::size_t>(i)];
                const Pos& b = worldLoop[static_cast<std::size_t>((i + 1) % 4)];
                const Pos& c = worldLoop[static_cast<std::size_t>((i + 2) % 4)];
                float t = 0.0f;
                if (not rayHitTriangle(ray.origin, ray.dir, a, b, c, t))
                    continue;
                const Pos hit = ray.origin + ray.dir * t;
                const Pos centroid{(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f};
                const float d2 = length2(hit - centroid);
                constexpr float epsT = 1e-5f;
                if (t < bestT - epsT or (std::abs(t - bestT) <= epsT and d2 < bestCentroidDist2)) {
                    bestT = t;
                    bestCentroidDist2 = d2;
                    best = i;
                }
            }
            if (best < 0)
                return quad;
            return {
                quad[static_cast<std::size_t>(best)],
                quad[static_cast<std::size_t>((best + 1) % 4)],
                quad[static_cast<std::size_t>((best + 2) % 4)],
            };
        }

        auto facePlate(const Cell& cell, mech::frame::FaceIndex face) -> base::maybe<mech::plate::shape> {
            const auto shapeIndex = static_cast<std::size_t>(cell.shape);
            const auto faceIndex = static_cast<std::size_t>(face);
            if (shapeIndex >= mech::skinning::default_plate.size() or faceIndex >= mech::skinning::default_plate[shapeIndex].size())
                return {};
            return mech::skinning::default_plate[shapeIndex][faceIndex];
        }

    } // namespace

    void resetAim(Cursor& cursor) {
        cursor.cell = {};
        cursor.face = {};
        cursor.points.clear();
    }

    void clearBalls(Writing context, scene::Root::Id root, Cursor& cursor) {
        for (const auto id : cursor.balls)
            destroyBall(context, root, id);
        cursor.balls.clear();
    }

    auto faceGridPoints(const Cell& cell, mech::frame::FaceIndex face) -> std::vector<base::common_types::index3> {
        const auto shapeIndex = static_cast<std::size_t>(cell.shape);
        const auto faceIndex = static_cast<std::size_t>(face);
        if (shapeIndex >= mech::frame::faces.size() or faceIndex >= mech::frame::faces[shapeIndex].size())
            return {};
        const auto ori = static_cast<mech::space::orient::key>(cell.placement.ori);
        const auto& cellPos = cell.placement.cell;
        std::vector<base::common_types::index3> out;
        out.reserve(mech::frame::faces[shapeIndex][faceIndex].size());
        for (const auto corner : mech::frame::faces[shapeIndex][faceIndex]) {
            const auto mapped = mech::space::orient::cornerIndex(ori, corner);
            const auto& lattice = mech::cube::corners[static_cast<std::size_t>(mapped)];
            out.push_back(base::common_types::index3{.x = cellPos.x + lattice.x, .y = cellPos.y + lattice.y, .z = cellPos.z + lattice.z});
        }
        return out;
    }

    auto shapeGridPoints(const Cell& cell) -> std::vector<base::common_types::index3> {
        const auto shapeIndex = static_cast<std::size_t>(cell.shape);
        if (shapeIndex >= mech::frame::corners.size())
            return {};
        const auto ori = static_cast<mech::space::orient::key>(cell.placement.ori);
        const auto& cellPos = cell.placement.cell;
        const auto& corners = mech::frame::corners[shapeIndex];
        std::vector<base::common_types::index3> out;
        out.reserve(corners.size());
        for (const auto corner : corners) {
            const auto mapped = mech::space::orient::cornerIndex(ori, corner);
            const auto& lattice = mech::cube::corners[static_cast<std::size_t>(mapped)];
            out.push_back(base::common_types::index3{.x = cellPos.x + lattice.x, .y = cellPos.y + lattice.y, .z = cellPos.z + lattice.z});
        }
        return out;
    }

    void aim(Cursor& cursor, const Blueprint& blueprint, const MouseRay& ray, bool wholeCell, bool altSquareTri) {
        resetAim(cursor);
        if (not cursor.enabled)
            return;
        base::maybe<std::size_t> bestCell;
        base::maybe<mech::frame::FaceIndex> bestFace;
        float bestT = std::numeric_limits<float>::infinity();
        for (std::size_t cellIndex = 0; cellIndex < blueprint.cells.size(); ++cellIndex) {
            const auto& cell = blueprint.cells[cellIndex];
            const auto shapeIndex = static_cast<std::size_t>(cell.shape);
            if (shapeIndex >= mech::frame::faces.size())
                continue;
            const auto& faces = mech::frame::faces[shapeIndex];
            for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                const auto face = static_cast<mech::frame::FaceIndex>(faceIndex);
                const auto loop = faceWorldLoop(cell, face);
                float t = 0.0f;
                if (rayHitPolygon(ray.origin, ray.dir, loop, t) and t < bestT) {
                    bestT = t;
                    bestCell = cellIndex;
                    bestFace = face;
                }
            }
        }
        if (not bestCell.has_value() or not bestFace.has_value())
            return;
        cursor.cell = *bestCell;
        const auto& cell = blueprint.cells[*bestCell];
        if (wholeCell) {
            cursor.points = shapeGridPoints(cell);
            return;
        }
        cursor.face = *bestFace;
        cursor.points = faceGridPoints(cell, *bestFace);
        if (altSquareTri and cursor.points.size() == 4) {
            const auto plate = facePlate(cell, *bestFace);
            if (plate.has_value() and *plate == mech::plate::shape::p1111)
                cursor.points = nearestSquareTriPoints(cursor.points, faceWorldLoop(cell, *bestFace), ray);
        }
    }

    void syncBalls(Writing context, scene::Root::Id root, Cursor& cursor) {
        if (not cursor.enabled or not cursor.sphere.has_value() or not cursor.material.has_value()) {
            clearBalls(context, root, cursor);
            return;
        }
        if (cursor.balls.size() != cursor.points.size()) {
            clearBalls(context, root, cursor);
            const auto resolved = meshpack::Asset::Resolved{
                .geometry = *cursor.sphere,
                .entry = geometry::EntryId{0},
                .surfaces = {{geometry::SurfaceId{0}, ::rmmr::resource::material::Instance{.material = *cursor.material, .textures = {}}}},
                .texpack = {},
            };
            const auto stateQuantum = with<scene::actor::MeshState>::defaults(ballAlbedo, ballOpacity, vec3{ballScale, ballScale, ballScale});
            for (const auto& point : cursor.points) {
                const auto id = with<scene::Interface>::createMeshActor(context, root, Pose::from(gridMeters(point), HPB{0.0f, 0.0f, 0.0f}), resolved, stateQuantum);
                if (with<scene::actor::Mesh>::exists(context, id))
                    cursor.balls.push_back(id);
            }
            return;
        }
        for (std::size_t i = 0; i < cursor.points.size(); ++i) {
            if (not with<scene::Node>::exists(context, cursor.balls[i]))
                continue;
            with<scene::Node>::modify(context, cursor.balls[i])->pose = Pose::from(gridMeters(cursor.points[i]), HPB{0.0f, 0.0f, 0.0f});
            if (with<scene::actor::MeshState>::exists(context, cursor.balls[i]))
                scene::actor::MeshState::Actions::setVisible(context, cursor.balls[i], true);
        }
    }

}
