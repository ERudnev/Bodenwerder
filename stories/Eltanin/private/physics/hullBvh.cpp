#include "physics/hullBvh.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
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
            // twoSided plates and 2-vert capsules offset the surface by thickness; one-sided rock faces use thickness only as a far-wall reject slab (COM depth), not a geometric shell.
            const float pad = (face.points.size() == 2 or face.twoSided) ? glm::max(face.thickness, 0.0f) : 0.0f;
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

        auto clipSegmentAabb(vec3 p0, vec3 p1, vec3 bmin, vec3 bmax, float pad) -> bool {
            const vec3 mn = bmin - vec3{pad};
            const vec3 mx = bmax + vec3{pad};
            const vec3 d = p1 - p0;
            float t0 = 0.0f;
            float t1 = 1.0f;
            for (int axis = 0; axis < 3; ++axis) {
                if (glm::abs(d[axis]) < minLength) {
                    if (p0[axis] < mn[axis] or p0[axis] > mx[axis])
                        return false;
                    continue;
                }
                const float inv = 1.0f / d[axis];
                float ta = (mn[axis] - p0[axis]) * inv;
                float tb = (mx[axis] - p0[axis]) * inv;
                if (ta > tb) {
                    const float tmp = ta;
                    ta = tb;
                    tb = tmp;
                }
                t0 = glm::max(t0, ta);
                t1 = glm::min(t1, tb);
                if (t0 > t1)
                    return false;
            }
            return true;
        }

        auto firstOnSphere(vec3 p0, vec3 p1, vec3 center, float radius) -> float {
            const vec3 d = p1 - p0;
            const vec3 f = p0 - center;
            const float r2 = radius * radius;
            const float c = glm::dot(f, f) - r2;
            if (c <= 0.0f)
                return 0.0f;
            const float a = glm::dot(d, d);
            if (a < minLength)
                return -1.0f;
            const float b = 2.0f * glm::dot(f, d);
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f)
                return -1.0f;
            const float t = (-b - std::sqrt(disc)) / (2.0f * a);
            if (t < 0.0f or t > 1.0f)
                return -1.0f;
            return t;
        }

        auto firstOnFiniteCylinder(vec3 p0, vec3 p1, vec3 a, vec3 b, float radius) -> float {
            const vec3 d = p1 - p0;
            const vec3 ba = b - a;
            const vec3 m = p0 - a;
            const float dd = glm::dot(ba, ba);
            if (dd < minLength)
                return -1.0f;
            const float nn = glm::dot(d, d);
            const float nd = glm::dot(d, ba);
            const float md = glm::dot(m, ba);
            const float mm = glm::dot(m, m);
            const float mn = glm::dot(m, d);
            const float r2 = radius * radius;
            const float A = dd * nn - nd * nd;
            const float C = dd * (mm - r2) - md * md;
            if (C <= 0.0f)
                return -1.0f;
            if (A < minLength)
                return -1.0f;
            const float B = dd * mn - nd * md;
            const float disc = B * B - A * C;
            if (disc < 0.0f)
                return -1.0f;
            const float t = (-B - std::sqrt(disc)) / A;
            if (t < 0.0f or t > 1.0f)
                return -1.0f;
            const float u = (md + t * nd) / dd;
            if (u < 0.0f or u > 1.0f)
                return -1.0f;
            return t;
        }

        auto firstOnCapsule(vec3 p0, vec3 p1, vec3 a, vec3 b, float radius) -> float {
            float best = 2.0f;
            const float ta = firstOnSphere(p0, p1, a, radius);
            const float tb = firstOnSphere(p0, p1, b, radius);
            const float tc = firstOnFiniteCylinder(p0, p1, a, b, radius);
            if (ta >= 0.0f and ta < best)
                best = ta;
            if (tb >= 0.0f and tb < best)
                best = tb;
            if (tc >= 0.0f and tc < best)
                best = tc;
            return best <= 1.0f ? best : -1.0f;
        }

        void considerSegment(SegmentHit& best, integer face, float t, vec3 closest, vec3 outward) {
            if (t < 0.0f or t > 1.0f or t >= best.t)
                return;
            const float length = glm::length(outward);
            if (length < minLength)
                return;
            best.face = face;
            best.localClosest = closest;
            best.localOutward = outward / length;
            best.t = t;
        }

        void firstOnFace(const Hull& hull, const vector<vec3>& shape, integer faceIndex, vec3 localStart, vec3 localEnd, float radius, SegmentHit& best) {
            if (faceIndex < 0 or static_cast<std::size_t>(faceIndex) >= hull.faces.size())
                return;
            const Hull::Face& face = hull.faces[static_cast<std::size_t>(faceIndex)];
            const std::size_t count = face.points.size();
            if (count < 2 or count > maxFaceVertices)
                return;
            vec3 vertices[maxFaceVertices];
            for (std::size_t index = 0; index < count; ++index) {
                if (face.points[index] < 0 or static_cast<std::size_t>(face.points[index]) >= shape.size())
                    return;
                vertices[index] = shape[static_cast<std::size_t>(face.points[index])];
            }
            const float shell = glm::max(face.thickness, 0.0f);
            const float skin = (count == 2 or face.twoSided) ? shell : 0.0f;
            const float reach = skin + glm::max(radius, 0.0f);
            const vec3 d = localEnd - localStart;
            if (count == 2) {
                const float t = firstOnCapsule(localStart, localEnd, vertices[0], vertices[1], reach);
                if (t < 0.0f)
                    return;
                const vec3 point = localStart + t * d;
                const vec3 axisClosest = closestOnSegment(point, vertices[0], vertices[1]);
                const vec3 outward = unitOrFallback(point - axisClosest, face.normal);
                considerSegment(best, faceIndex, t, axisClosest + outward * shell, outward);
                return;
            }
            vec3 normal = face.normal;
            float length = glm::length(normal);
            if (length < minLength) {
                normal = glm::cross(vertices[1] - vertices[0], vertices[2] - vertices[0]);
                length = glm::length(normal);
                if (length < minLength)
                    return;
            }
            normal /= length;
            const float denom = glm::dot(d, normal);
            const float dist0 = glm::dot(localStart - vertices[0], normal);
            const float offsets[2] = {radius, -radius};
            const vec3 sides[2] = {normal, -normal};
            const int planes = face.twoSided ? 2 : 1;
            for (int plane = 0; plane < planes; ++plane) {
                if (glm::abs(denom) < minLength)
                    break;
                const float t = (offsets[plane] - dist0) / denom;
                const vec3 point = localStart + t * d;
                const vec3 projected = point - glm::dot(point - vertices[0], normal) * normal;
                if (not projectsInside(vertices, count, projected, normal))
                    continue;
                considerSegment(best, faceIndex, t, projected, sides[plane]);
            }
            for (std::size_t index = 0; index < count; ++index) {
                const vec3& a = vertices[index];
                const vec3& b = vertices[(index + 1) % count];
                const float t = firstOnCapsule(localStart, localEnd, a, b, reach);
                if (t < 0.0f)
                    continue;
                const vec3 point = localStart + t * d;
                const vec3 axisClosest = closestOnSegment(point, a, b);
                const vec3 outward = face.twoSided ? unitOrFallback(point - axisClosest, normal) : normal;
                considerSegment(best, faceIndex, t, axisClosest, outward);
            }
        }

        void visitSegment(const Hull::Bvh& bvh, const Hull& hull, const vector<vec3>& shape, vec3 localStart, vec3 localEnd, float radius, integer nodeIndex, SegmentHit& hit) {
            if (nodeIndex < 0)
                return;
            const Hull::Bvh::Node& node = bvh.nodes[static_cast<std::size_t>(nodeIndex)];
            if (not clipSegmentAabb(localStart, localEnd, node.boundMin, node.boundMax, glm::max(radius, 0.0f)))
                return;
            if (node.face >= 0) {
                firstOnFace(hull, shape, node.face, localStart, localEnd, radius, hit);
                return;
            }
            visitSegment(bvh, hull, shape, localStart, localEnd, radius, node.left, hit);
            visitSegment(bvh, hull, shape, localStart, localEnd, radius, node.right, hit);
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
        if (planeDistance < -shell and not face.twoSided)
            return false;
        const vec3 side = face.twoSided and planeDistance < 0.0f ? -normal : normal;
        const vec3 projected = localPoint - planeDistance * normal;
        if (projectsInside(vertices, count, projected, normal)) {
            localOutward = side;
            localClosest = face.twoSided ? projected + side * shell : projected;
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
        if (face.twoSided) {
            localOutward = unitOrFallback(localPoint - localClosest, side);
            localClosest += localOutward * shell;
            return true;
        }
        localOutward = side;
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

    auto firstOnHull(const Hull& hull, const vector<vec3>& shape, vec3 localStart, vec3 localEnd, float radius) -> SegmentHit {
        SegmentHit hit{.face = -1, .localClosest = localStart, .localOutward = vec3{0.0f, 1.0f, 0.0f}, .t = 2.0f};
        if (hull.bvh.nodes.empty() or hull.bvh.root < 0)
            return hit;
        const Hull::Bvh::Node& root = hull.bvh.nodes[static_cast<std::size_t>(hull.bvh.root)];
        const float reach = glm::max(radius, 0.0f);
        if (aabbDistance2(localStart, root.boundMin, root.boundMax) <= reach * reach) {
            const SurfaceHit near = closestOnHull(hull, shape, localStart);
            if (near.face >= 0 and near.signedDistance < radius) {
                hit.face = near.face;
                hit.localClosest = near.localClosest;
                hit.localOutward = near.localOutward;
                hit.t = 0.0f;
                return hit;
            }
        }
        visitSegment(hull.bvh, hull, shape, localStart, localEnd, radius, hull.bvh.root, hit);
        return hit;
    }

}
