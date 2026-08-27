#include "physics/hullBvh.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace eltanin::phys::collision {

    using rigid::Hull;

    namespace {

        constexpr float minLength = 1.0e-8f;
        constexpr std::size_t maxFaceVertices = 8;

        auto faceBound(const Hull::Face& face, const vector<vec3>& shape, vec3& boundMin, vec3& boundMax) -> bool {
            bool any = false;
            for (const integer id : face.points) {
                if (id < 0 or static_cast<std::size_t>(id) >= shape.size())
                    return false;
                const vec3& point = shape[static_cast<std::size_t>(id)];
                if (not any) {
                    boundMin = point;
                    boundMax = point;
                    any = true;
                    continue;
                }
                boundMin = glm::min(boundMin, point);
                boundMax = glm::max(boundMax, point);
            }
            if (not any)
                return false;
            const float pad = glm::max(face.thickness, 0.0f);
            boundMin -= vec3{pad, pad, pad};
            boundMax += vec3{pad, pad, pad};
            return true;
        }

        auto unitOrFallback(vec3 candidate, vec3 fallback) -> vec3 {
            const float length = glm::length(candidate);
            if (length >= minLength)
                return candidate / length;
            const float fallbackLength = glm::length(fallback);
            if (fallbackLength >= minLength)
                return fallback / fallbackLength;
            return vec3{0.0f, 1.0f, 0.0f};
        }

        auto faceCentroid(const Hull::Face& face, const vector<vec3>& shape) -> vec3 {
            vec3 sum{0.0f, 0.0f, 0.0f};
            float count = 0.0f;
            for (const integer id : face.points) {
                if (id < 0 or static_cast<std::size_t>(id) >= shape.size())
                    continue;
                sum += shape[static_cast<std::size_t>(id)];
                count += 1.0f;
            }
            return count > 0.0f ? sum / count : vec3{0.0f, 0.0f, 0.0f};
        }

        auto longestAxis(vec3 boundMin, vec3 boundMax) -> int {
            const vec3 extent = boundMax - boundMin;
            if (extent.y > extent.x and extent.y >= extent.z)
                return 1;
            if (extent.z > extent.x and extent.z >= extent.y)
                return 2;
            return 0;
        }

        auto aabbDistance2(vec3 point, vec3 boundMin, vec3 boundMax) -> float {
            const vec3 delta = glm::max(boundMin - point, glm::max(vec3{0.0f, 0.0f, 0.0f}, point - boundMax));
            return glm::dot(delta, delta);
        }

        auto closestOnSegment(vec3 point, vec3 start, vec3 end) -> vec3 {
            const vec3 span = end - start;
            const float denom = glm::dot(span, span);
            if (denom <= minLength)
                return start;
            const float t = glm::clamp(glm::dot(point - start, span) / denom, 0.0f, 1.0f);
            return start + t * span;
        }

        auto projectsInside(const vec3* vertices, std::size_t count, vec3 projected, vec3 normal) -> bool {
            for (std::size_t index = 0; index < count; ++index) {
                const vec3 edge = vertices[(index + 1) % count] - vertices[index];
                const vec3 toPoint = projected - vertices[index];
                if (glm::dot(glm::cross(edge, toPoint), normal) < -minLength)
                    return false;
            }
            return true;
        }

        auto build(Hull::Bvh& bvh, const Hull& hull, const vector<vec3>& shape, vector<integer>& indices, std::size_t begin, std::size_t end) -> integer {
            vec3 boundMin;
            vec3 boundMax;
            bool any = false;
            for (std::size_t index = begin; index < end; ++index) {
                vec3 faceMin;
                vec3 faceMax;
                if (not faceBound(hull.faces[static_cast<std::size_t>(indices[index])], shape, faceMin, faceMax))
                    continue;
                if (not any) {
                    boundMin = faceMin;
                    boundMax = faceMax;
                    any = true;
                    continue;
                }
                boundMin = glm::min(boundMin, faceMin);
                boundMax = glm::max(boundMax, faceMax);
            }
            if (not any)
                return -1;
            if (end - begin == 1) {
                bvh.nodes.push_back(Hull::Bvh::Node{.boundMin = boundMin, .boundMax = boundMax, .left = -1, .right = -1, .face = indices[begin]});
                return static_cast<integer>(bvh.nodes.size() - 1);
            }
            const std::size_t mid = begin + (end - begin) / 2;
            const int axis = longestAxis(boundMin, boundMax);
            std::nth_element(indices.begin() + static_cast<std::ptrdiff_t>(begin), indices.begin() + static_cast<std::ptrdiff_t>(mid), indices.begin() + static_cast<std::ptrdiff_t>(end), [&](integer faceA, integer faceB) {
                return faceCentroid(hull.faces[static_cast<std::size_t>(faceA)], shape)[axis] < faceCentroid(hull.faces[static_cast<std::size_t>(faceB)], shape)[axis];
            });
            const integer left = build(bvh, hull, shape, indices, begin, mid);
            const integer right = build(bvh, hull, shape, indices, mid, end);
            if (left < 0)
                return right;
            if (right < 0)
                return left;
            boundMin = glm::min(bvh.nodes[static_cast<std::size_t>(left)].boundMin, bvh.nodes[static_cast<std::size_t>(right)].boundMin);
            boundMax = glm::max(bvh.nodes[static_cast<std::size_t>(left)].boundMax, bvh.nodes[static_cast<std::size_t>(right)].boundMax);
            bvh.nodes.push_back(Hull::Bvh::Node{.boundMin = boundMin, .boundMax = boundMax, .left = left, .right = right, .face = -1});
            return static_cast<integer>(bvh.nodes.size() - 1);
        }

        void visit(const Hull::Bvh& bvh, const Hull& hull, const vector<vec3>& shape, vec3 localPoint, integer nodeIndex, float& bestDist2, SurfaceHit& hit) {
            if (nodeIndex < 0)
                return;
            const Hull::Bvh::Node& node = bvh.nodes[static_cast<std::size_t>(nodeIndex)];
            if (aabbDistance2(localPoint, node.boundMin, node.boundMax) > bestDist2)
                return;
            if (node.face >= 0) {
                vec3 closest;
                vec3 outward;
                if (not closestOnFace(hull, shape, node.face, localPoint, closest, outward))
                    return;
                const vec3 delta = localPoint - closest;
                const float dist2 = glm::dot(delta, delta);
                if (dist2 >= bestDist2)
                    return;
                bestDist2 = dist2;
                hit.face = node.face;
                hit.localClosest = closest;
                hit.localOutward = outward;
                hit.signedDistance = glm::dot(delta, outward);
                return;
            }
            const float distLeft = node.left >= 0 ? aabbDistance2(localPoint, bvh.nodes[static_cast<std::size_t>(node.left)].boundMin, bvh.nodes[static_cast<std::size_t>(node.left)].boundMax) : std::numeric_limits<float>::infinity();
            const float distRight = node.right >= 0 ? aabbDistance2(localPoint, bvh.nodes[static_cast<std::size_t>(node.right)].boundMin, bvh.nodes[static_cast<std::size_t>(node.right)].boundMax) : std::numeric_limits<float>::infinity();
            if (distLeft < distRight) {
                visit(bvh, hull, shape, localPoint, node.left, bestDist2, hit);
                visit(bvh, hull, shape, localPoint, node.right, bestDist2, hit);
            } else {
                visit(bvh, hull, shape, localPoint, node.right, bestDist2, hit);
                visit(bvh, hull, shape, localPoint, node.left, bestDist2, hit);
            }
        }

    }

    void cookHullBvh(Hull& hull, const vector<vec3>& shape) {
        hull.bvh.nodes.clear();
        hull.bvh.root = -1;
        vector<integer> indices;
        indices.reserve(hull.faces.size());
        for (std::size_t index = 0; index < hull.faces.size(); ++index) {
            vec3 boundMin;
            vec3 boundMax;
            if (not faceBound(hull.faces[index], shape, boundMin, boundMax))
                continue;
            indices.push_back(static_cast<integer>(index));
        }
        if (indices.empty())
            return;
        hull.bvh.nodes.reserve(indices.size() * 2);
        hull.bvh.root = build(hull.bvh, hull, shape, indices, 0, indices.size());
    }

    auto closestOnFace(const Hull& hull, const vector<vec3>& shape, integer faceIndex, vec3 localPoint, vec3& localClosest, vec3& localOutward) -> bool {
        if (faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= hull.faces.size())
            return false;
        const Hull::Face& face = hull.faces[static_cast<std::size_t>(faceIndex)];
        const std::size_t count = face.points.size();
        if (count < 2 or count > maxFaceVertices)
            return false;
        vec3 vertices[maxFaceVertices];
        for (std::size_t index = 0; index < count; ++index) {
            if (face.points[index] < 0 or static_cast<std::size_t>(face.points[index]) >= shape.size())
                return false;
            vertices[index] = shape[static_cast<std::size_t>(face.points[index])];
        }
        const float shell = glm::max(face.thickness, 0.0f);
        if (count == 2) {
            const vec3 axisClosest = closestOnSegment(localPoint, vertices[0], vertices[1]);
            localOutward = unitOrFallback(localPoint - axisClosest, face.normal);
            localClosest = axisClosest + localOutward * shell;
            return true;
        }
        vec3 normal = face.normal;
        float length = glm::length(normal);
        if (length < minLength) {
            normal = glm::cross(vertices[1] - vertices[0], vertices[2] - vertices[0]);
            length = glm::length(normal);
            if (length < minLength)
                return false;
        }
        normal /= length;
        const float planeDistance = glm::dot(localPoint - vertices[0], normal);
        if (planeDistance < -shell)
            return false;
        const vec3 side = face.twoSided and planeDistance < 0.0f ? -normal : normal;
        const vec3 projected = localPoint - planeDistance * normal;
        if (projectsInside(vertices, count, projected, normal)) {
            localClosest = projected;
            localOutward = side;
            return true;
        }
        localClosest = closestOnSegment(localPoint, vertices[count - 1], vertices[0]);
        float bestDist2 = glm::dot(localPoint - localClosest, localPoint - localClosest);
        for (std::size_t index = 0; index + 1 < count; ++index) {
            const vec3 candidate = closestOnSegment(localPoint, vertices[index], vertices[index + 1]);
            const vec3 delta = localPoint - candidate;
            const float dist2 = glm::dot(delta, delta);
            if (dist2 >= bestDist2)
                continue;
            bestDist2 = dist2;
            localClosest = candidate;
        }
        localOutward = face.twoSided ? unitOrFallback(localPoint - localClosest, side) : normal;
        return true;
    }

    auto closestOnHull(const Hull& hull, const vector<vec3>& shape, vec3 localPoint) -> SurfaceHit {
        SurfaceHit hit{.face = -1, .localClosest = localPoint, .localOutward = vec3{0.0f, 1.0f, 0.0f}, .signedDistance = 0.0f};
        if (hull.bvh.nodes.empty() or hull.bvh.root < 0)
            return hit;
        float bestDist2 = std::numeric_limits<float>::infinity();
        visit(hull.bvh, hull, shape, localPoint, hull.bvh.root, bestDist2, hit);
        return hit;
    }

}
