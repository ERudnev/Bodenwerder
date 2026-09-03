#include "mech/construction.h"

#include <map>
#include <set>

#include <glm/geometric.hpp>

namespace eltanin::mech {

    using namespace fqsm::api;

    namespace {

        struct LatticeLess {
            auto operator()(const index3& left, const index3& right) const -> bool {
                if (left.x != right.x) return left.x < right.x;
                if (left.y != right.y) return left.y < right.y;
                return left.z < right.z;
            }
        };

        using EdgeKey = std::pair<index3, index3>;

        struct EdgeLess {
            auto operator()(const EdgeKey& left, const EdgeKey& right) const -> bool {
                if (LatticeLess{}(left.first, right.first)) return true;
                if (LatticeLess{}(right.first, left.first)) return false;
                return LatticeLess{}(left.second, right.second);
            }
        };

        auto sameLattice(const index3& a, const index3& b) -> bool {
            return a.x == b.x and a.y == b.y and a.z == b.z;
        }

        auto canonicalEdge(index3 a, index3 b) -> EdgeKey {
            if (LatticeLess{}(b, a)) return EdgeKey{b, a};
            return EdgeKey{a, b};
        }

        auto loopGrid(const Construction::Primitive& primitive) -> vector<index3> {
            vector<index3> grid;
            grid.reserve(primitive.loop.size());
            for (const auto& welded : primitive.loop)
                grid.push_back(welded.gridPos);
            return grid;
        }

        void addLoopEdges(const Construction::Primitive& primitive, std::set<EdgeKey, EdgeLess>& covered) {
            const auto grid = loopGrid(primitive);
            if (grid.size() == 2) {
                covered.insert(canonicalEdge(grid[0], grid[1]));
                return;
            }
            if (grid.size() < 3)
                return;
            for (std::size_t index = 0; index < grid.size(); ++index)
                covered.insert(canonicalEdge(grid[index], grid[(index + 1) % grid.size()]));
        }

        auto sortedKnots(vector<integer> points) -> vector<integer> {
            std::sort(points.begin(), points.end());
            return points;
        }

        auto beamFace(integer start, integer end, const vector<vec3>& shape, float thickness) -> phys::rigid::Hull::Face {
            const vec3 edge = shape[static_cast<std::size_t>(end)] - shape[static_cast<std::size_t>(start)];
            const vec3 mid = 0.5f * (shape[static_cast<std::size_t>(start)] + shape[static_cast<std::size_t>(end)]);
            vec3 normal = glm::cross(edge, mid);
            if (glm::dot(normal, normal) < 1.0e-12f)
                normal = glm::cross(edge, vec3{1.0f, 0.0f, 0.0f});
            if (glm::dot(normal, normal) < 1.0e-12f)
                normal = glm::cross(edge, vec3{0.0f, 1.0f, 0.0f});
            const float mag = glm::length(normal);
            if (mag > 1.0e-12f)
                normal /= mag;
            else
                normal = vec3{0.0f, 1.0f, 0.0f};
            return phys::rigid::Hull::Face{.points = {start, end}, .normal = normal, .thickness = thickness, .twoSided = false};
        }

        auto plateFace(const vector<integer>& points, const vector<vec3>& shape, float thickness) -> phys::rigid::Hull::Face {
            if (points.size() < 3)
                return phys::rigid::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}, .thickness = thickness, .twoSided = false};
            const vec3 ab = shape[static_cast<std::size_t>(points[1])] - shape[static_cast<std::size_t>(points[0])];
            const vec3 ac = shape[static_cast<std::size_t>(points[2])] - shape[static_cast<std::size_t>(points[0])];
            vec3 normal = glm::cross(ab, ac);
            const float mag = glm::length(normal);
            if (mag <= 1.0e-12f)
                return phys::rigid::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}, .thickness = thickness, .twoSided = false};
            normal /= mag;
            return phys::rigid::Hull::Face{.points = points, .normal = normal, .thickness = thickness, .twoSided = true};
        }

        auto hullFace(const vector<integer>& points, const vector<vec3>& shape, float thickness) -> phys::rigid::Hull::Face {
            if (points.size() == 2)
                return beamFace(points[0], points[1], shape, thickness);
            return plateFace(points, shape, thickness);
        }

    }

    auto cookHull(const Construction& construction, const vector<vec3>& shape) -> phys::rigid::Hull {
        using Primitive = Construction::Primitive;
        using PrimitiveId = Primitive::Id;
        std::set<EdgeKey, EdgeLess> covered;
        for (const auto& [_, primitive] : construction.membranes)
            addLoopEdges(primitive, covered);
        for (const auto& [_, primitive] : construction.plates)
            addLoopEdges(primitive, covered);
        for (const auto& [_, faces] : construction.volumes) {
            for (const auto& primitive : faces)
                addLoopEdges(primitive, covered);
        }
        phys::rigid::Hull hull{.faces = {}, .bvh = {.nodes = {}, .root = -1}};
        hull.faces.reserve(construction.ribs.size() + construction.membranes.size() + construction.plates.size());
        integer cursor = 0;
        auto takeLoop = [&](const Primitive& primitive) {
            vector<integer> points;
            points.reserve(primitive.loop.size());
            while (points.size() < primitive.loop.size())
                points.push_back(cursor++);
            return points;
        };
        auto pushPolygon = [&](const Primitive& primitive, const vector<integer>& points) {
            if (primitive.loop.size() < 2 or points.size() != primitive.loop.size())
                return;
            auto face = hullFace(points, shape, primitive.thickness);
            if (face.points.empty())
                return;
            const auto key = sortedKnots(points);
            for (auto& existing : hull.faces) {
                if (sortedKnots(existing.points) != key)
                    continue;
                if (primitive.thickness > existing.thickness)
                    existing = std::move(face);
                return;
            }
            hull.faces.push_back(std::move(face));
        };
        forEachPrimitiveLoop(construction, [&](PrimitiveId id, const Primitive& primitive) {
            const auto points = takeLoop(primitive);
            if (construction.knots.find(id) != construction.knots.end())
                return;
            if (construction.ribs.find(id) != construction.ribs.end()) {
                const auto grid = loopGrid(primitive);
                if (grid.size() != 2 or sameLattice(grid[0], grid[1]))
                    return;
                if (covered.find(canonicalEdge(grid[0], grid[1])) != covered.end())
                    return;
                if (points.size() != 2)
                    return;
                hull.faces.push_back(beamFace(points[0], points[1], shape, primitive.thickness));
                return;
            }
            pushPolygon(primitive, points);
        });
        return hull;
    }

    void bindKnotWelds(Construction& construction) {
        std::map<index3, Construction::Knot*, LatticeLess> at;
        for (auto& [_, knot] : construction.knots) {
            knot.welded.clear();
            if (knot.loop.empty())
                continue;
            at.emplace(knot.loop[0].gridPos, &knot);
        }
        for (std::size_t index = 0; index < construction.evaluatedParticles.size(); ++index) {
            auto found = at.find(construction.evaluatedParticles[index].gridPos);
            if (found == at.end())
                continue;
            found->second->welded.push_back(static_cast<integer>(index));
        }
    }

}
