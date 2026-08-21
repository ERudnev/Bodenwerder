#include "physics/compound.h"
#include "mech/semantics/space.h"

#include <rmmr/semantics/geometry.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <glm/geometric.hpp>

namespace eltanin::phys::rigid {

    using namespace rmmr;
    using rmmr::resource::builders::geometry::CpuPresentation;

    namespace {

        constexpr float hullEps = 1.0e-6f;

        struct Triangle {
            integer verts[3];
        };

        auto edgeCells(integer scale) -> integer {
            return 1 << scale;
        }

        auto triangleNormal(const vector<vec3>& points, Triangle triangle) -> vec3 {
            const vec3 edgeB = points[static_cast<std::size_t>(triangle.verts[1])] - points[static_cast<std::size_t>(triangle.verts[0])];
            const vec3 edgeC = points[static_cast<std::size_t>(triangle.verts[2])] - points[static_cast<std::size_t>(triangle.verts[0])];
            return glm::cross(edgeB, edgeC);
        }

        auto makeOutward(integer vert0, integer vert1, integer vert2, const vector<vec3>& points, vec3 hint) -> Triangle {
            Triangle triangle{vert0, vert1, vert2};
            const vec3 normal = triangleNormal(points, triangle);
            const vec3 centroid = (points[static_cast<std::size_t>(vert0)] + points[static_cast<std::size_t>(vert1)] + points[static_cast<std::size_t>(vert2)]) / 3.0f;
            if (glm::dot(normal, centroid - hint) < 0.0f) {
                triangle.verts[1] = vert2;
                triangle.verts[2] = vert1;
            }
            return triangle;
        }

        auto faceFromTriangle(Triangle triangle, const vector<vec3>& points, const vector<integer>& ids) -> Compound::Hull::Face {
            const vec3 normal = glm::normalize(triangleNormal(points, triangle));
            return Compound::Hull::Face{
                .points = {ids[static_cast<std::size_t>(triangle.verts[0])], ids[static_cast<std::size_t>(triangle.verts[1])], ids[static_cast<std::size_t>(triangle.verts[2])]},
                .normal = normal,
            };
        }

        auto convexFace2d(const vector<integer>& ids, const vector<vec3>& shape, vec3 hint) -> Compound::Hull::Face {
            if (ids.size() < 3)
                return Compound::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}};
            vec3 origin = shape[static_cast<std::size_t>(ids[0])];
            vec3 axis = vec3{0.0f, 0.0f, 0.0f};
            for (std::size_t index = 1; index < ids.size(); ++index) {
                axis = shape[static_cast<std::size_t>(ids[index])] - origin;
                if (glm::dot(axis, axis) > hullEps * hullEps)
                    break;
            }
            vec3 normal = vec3{0.0f, 0.0f, 0.0f};
            for (std::size_t index = 2; index < ids.size(); ++index) {
                normal = glm::cross(axis, shape[static_cast<std::size_t>(ids[index])] - origin);
                if (glm::dot(normal, normal) > hullEps * hullEps)
                    break;
            }
            if (glm::dot(normal, normal) <= hullEps * hullEps)
                return Compound::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}};
            normal = glm::normalize(normal);
            vec3 centroid{0.0f, 0.0f, 0.0f};
            for (const integer id : ids)
                centroid += shape[static_cast<std::size_t>(id)];
            centroid /= static_cast<float>(ids.size());
            if (glm::dot(normal, centroid - hint) < 0.0f)
                normal = -normal;
            const vec3 tangent = glm::normalize(axis - normal * glm::dot(axis, normal));
            const vec3 bitangent = glm::cross(normal, tangent);
            struct Projected {
                integer id;
                float x;
                float y;
            };
            vector<Projected> projected;
            projected.reserve(ids.size());
            for (const integer id : ids) {
                const vec3 delta = shape[static_cast<std::size_t>(id)] - origin;
                projected.push_back(Projected{.id = id, .x = glm::dot(delta, tangent), .y = glm::dot(delta, bitangent)});
            }
            std::sort(projected.begin(), projected.end(), [](const Projected& left, const Projected& right) {
                if (left.x != right.x) return left.x < right.x;
                return left.y < right.y;
            });
            auto cross2 = [](const Projected& originPt, const Projected& a, const Projected& b) {
                return (a.x - originPt.x) * (b.y - originPt.y) - (a.y - originPt.y) * (b.x - originPt.x);
            };
            vector<Projected> hull;
            hull.reserve(projected.size() * 2);
            for (const Projected& point : projected) {
                while (hull.size() >= 2 and cross2(hull[hull.size() - 2], hull.back(), point) <= 0.0f)
                    hull.pop_back();
                hull.push_back(point);
            }
            const std::size_t lower = hull.size();
            for (std::size_t index = projected.size(); index-- > 0;) {
                const Projected& point = projected[index];
                while (hull.size() > lower and cross2(hull[hull.size() - 2], hull.back(), point) <= 0.0f)
                    hull.pop_back();
                hull.push_back(point);
            }
            if (not hull.empty())
                hull.pop_back();
            Compound::Hull::Face face{.points = {}, .normal = normal};
            face.points.reserve(hull.size());
            for (const Projected& point : hull)
                face.points.push_back(point.id);
            return face;
        }

        auto convexFaces(const vector<integer>& ids, const vector<vec3>& shape, vec3 hint) -> vector<Compound::Hull::Face> {
            if (ids.size() < 3)
                return {};
            vector<vec3> points;
            points.reserve(ids.size());
            for (const integer id : ids)
                points.push_back(shape[static_cast<std::size_t>(id)]);

            integer vert0 = 0;
            integer vert1 = -1;
            integer vert2 = -1;
            integer vert3 = -1;
            for (integer index = 1; index < static_cast<integer>(points.size()); ++index) {
                if (glm::length(points[static_cast<std::size_t>(index)] - points[0]) > hullEps) {
                    vert1 = index;
                    break;
                }
            }
            if (vert1 < 0)
                return {};
            for (integer index = 1; index < static_cast<integer>(points.size()); ++index) {
                if (index == vert1)
                    continue;
                const vec3 normal = glm::cross(points[static_cast<std::size_t>(vert1)] - points[0], points[static_cast<std::size_t>(index)] - points[0]);
                if (glm::dot(normal, normal) > hullEps * hullEps) {
                    vert2 = index;
                    break;
                }
            }
            if (vert2 < 0)
                return {};
            float bestVolume = 0.0f;
            for (integer index = 1; index < static_cast<integer>(points.size()); ++index) {
                if (index == vert1 or index == vert2)
                    continue;
                const vec3 normal = glm::cross(points[static_cast<std::size_t>(vert1)] - points[0], points[static_cast<std::size_t>(vert2)] - points[0]);
                const float volume = std::abs(glm::dot(normal, points[static_cast<std::size_t>(index)] - points[0]));
                if (volume > bestVolume) {
                    bestVolume = volume;
                    vert3 = index;
                }
            }
            if (vert3 < 0 or bestVolume <= hullEps * glm::length(points[static_cast<std::size_t>(vert1)] - points[0])) {
                const Compound::Hull::Face face = convexFace2d(ids, shape, hint);
                if (face.points.size() < 3)
                    return {};
                return {face};
            }

            vector<Triangle> faces;
            faces.push_back(makeOutward(vert0, vert1, vert2, points, hint));
            faces.push_back(makeOutward(vert0, vert2, vert3, points, hint));
            faces.push_back(makeOutward(vert0, vert3, vert1, points, hint));
            faces.push_back(makeOutward(vert1, vert3, vert2, points, hint));

            for (integer index = 0; index < static_cast<integer>(points.size()); ++index) {
                if (index == vert0 or index == vert1 or index == vert2 or index == vert3)
                    continue;
                vector<char> visible(faces.size(), 0);
                bool any = false;
                for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                    const vec3 normal = triangleNormal(points, faces[faceIndex]);
                    const float mag = glm::length(normal);
                    if (mag <= hullEps)
                        continue;
                    if (glm::dot(normal, points[static_cast<std::size_t>(index)] - points[static_cast<std::size_t>(faces[faceIndex].verts[0])]) > hullEps * mag) {
                        visible[faceIndex] = 1;
                        any = true;
                    }
                }
                if (not any)
                    continue;
                vector<std::pair<integer, integer>> horizon;
                for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                    if (not visible[faceIndex])
                        continue;
                    const Triangle& triangle = faces[faceIndex];
                    const integer edges[3][2] = {{triangle.verts[0], triangle.verts[1]}, {triangle.verts[1], triangle.verts[2]}, {triangle.verts[2], triangle.verts[0]}};
                    for (const auto& edge : edges) {
                        bool shared = false;
                        for (std::size_t other = 0; other < faces.size(); ++other) {
                            if (other == faceIndex or not visible[other])
                                continue;
                            const Triangle& otherTri = faces[other];
                            const integer otherEdges[3][2] = {{otherTri.verts[0], otherTri.verts[1]}, {otherTri.verts[1], otherTri.verts[2]}, {otherTri.verts[2], otherTri.verts[0]}};
                            for (const auto& otherEdge : otherEdges) {
                                if (edge[0] == otherEdge[1] and edge[1] == otherEdge[0])
                                    shared = true;
                            }
                        }
                        if (not shared)
                            horizon.push_back({edge[0], edge[1]});
                    }
                }
                vector<Triangle> next;
                next.reserve(faces.size() + horizon.size());
                for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
                    if (not visible[faceIndex])
                        next.push_back(faces[faceIndex]);
                }
                for (const auto& edge : horizon)
                    next.push_back(makeOutward(edge.first, edge.second, index, points, hint));
                faces = std::move(next);
            }

            vector<Compound::Hull::Face> result;
            result.reserve(faces.size());
            for (const Triangle& triangle : faces) {
                const vec3 normal = triangleNormal(points, triangle);
                if (glm::dot(normal, normal) <= hullEps * hullEps)
                    continue;
                result.push_back(faceFromTriangle(triangle, points, ids));
            }
            return result;
        }

        auto hullOf(const vector<integer>& ids, const vector<vec3>& shape, vec3 hint) -> Compound::Hull {
            return Compound::Hull{.faces = convexFaces(ids, shape, hint)};
        }

        void collectSolidLeaves(const geo::Volume& node, vector<const geo::Volume*>& leaves) {
            if (node.children.empty()) {
                if (node.mix != 0)
                    leaves.push_back(&node);
                return;
            }
            for (const geo::Volume& child : node.children)
                collectSolidLeaves(child, leaves);
        }

        auto pointInAabb(vec3 point, vec3 aabbMin, vec3 aabbMax, float pad) -> bool {
            return point.x + pad >= aabbMin.x and point.x - pad <= aabbMax.x
                and point.y + pad >= aabbMin.y and point.y - pad <= aabbMax.y
                and point.z + pad >= aabbMin.z and point.z - pad <= aabbMax.z;
        }

    }

    auto octaCompound() -> Compound {
        constexpr integer plusX = 0, minusX = 1, plusY = 2, minusY = 3, plusZ = 4, minusZ = 5;
        const integer tris[8][3] = {
            {plusX, plusY, plusZ}, {plusY, minusX, plusZ}, {minusX, minusY, plusZ}, {minusY, plusX, plusZ},
            {plusX, minusZ, plusY}, {plusY, minusZ, minusX}, {minusX, minusZ, minusY}, {minusY, minusZ, plusX},
        };
        const vec3 locals[6] = {
            vec3{1.0f, 0.0f, 0.0f}, vec3{-1.0f, 0.0f, 0.0f},
            vec3{0.0f, 1.0f, 0.0f}, vec3{0.0f, -1.0f, 0.0f},
            vec3{0.0f, 0.0f, 1.0f}, vec3{0.0f, 0.0f, -1.0f},
        };
        Compound::Hull hull{.faces = {}};
        hull.faces.reserve(8);
        for (const auto& tri : tris) {
            const vec3 a = locals[tri[0]];
            const vec3 b = locals[tri[1]];
            const vec3 c = locals[tri[2]];
            vec3 normal = glm::cross(b - a, c - a);
            if (glm::dot(normal, (a + b + c) / 3.0f) < 0.0f)
                hull.faces.push_back(Compound::Hull::Face{.points = {tri[0], tri[2], tri[1]}, .normal = glm::normalize(-normal)});
            else
                hull.faces.push_back(Compound::Hull::Face{.points = {tri[0], tri[1], tri[2]}, .normal = glm::normalize(normal)});
        }
        return Compound{.hulls = {std::move(hull)}};
    }

    auto wrapCompound(const vector<vec3>& shape, vec3 restCom) -> Compound {
        vector<integer> ids;
        ids.reserve(shape.size());
        for (std::size_t index = 0; index < shape.size(); ++index)
            ids.push_back(static_cast<integer>(index));
        Compound::Hull hull = hullOf(ids, shape, restCom);
        if (hull.faces.empty())
            return Compound{.hulls = {}};
        return Compound{.hulls = {std::move(hull)}};
    }

    auto volumeCompound(const geo::Volume& volume, const vector<vec3>& shape, vec3 restCom) -> Compound {
        vector<const geo::Volume*> leaves;
        collectSolidLeaves(volume, leaves);
        Compound compound{.hulls = {}};
        compound.hulls.reserve(leaves.size());
        const float pad = 0.1f;
        const float meters = mech::space::local::edge2meters;
        for (const geo::Volume* leaf : leaves) {
            const integer edge = edgeCells(leaf->scale);
            const vec3 aabbMin = vec3{static_cast<float>(leaf->origin.x), static_cast<float>(leaf->origin.y), static_cast<float>(leaf->origin.z)} * meters;
            const vec3 aabbMax = vec3{static_cast<float>(leaf->origin.x + edge), static_cast<float>(leaf->origin.y + edge), static_cast<float>(leaf->origin.z + edge)} * meters;
            vector<integer> ids;
            for (std::size_t index = 0; index < shape.size(); ++index) {
                if (glm::length(shape[index] - restCom) < 1.0e-4f)
                    continue;
                if (pointInAabb(shape[index], aabbMin, aabbMax, pad))
                    ids.push_back(static_cast<integer>(index));
            }
            Compound::Hull hull = hullOf(ids, shape, restCom);
            if (not hull.faces.empty())
                compound.hulls.push_back(std::move(hull));
        }
        if (compound.hulls.empty())
            return wrapCompound(shape, restCom);
        return compound;
    }

    auto debugMeshFromCompound(const Compound& compound, const vector<vec3>& shape) -> CpuPresentation {
        CpuPresentation cpu{
            .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"}),
            .positions = {},
            .normals = {},
            .uv0 = {},
            .color0 = {},
            .indices = {},
            .mix0 = {},
        };
        for (const Compound::Hull& hull : compound.hulls) {
            for (const Compound::Hull::Face& face : hull.faces) {
                if (face.points.size() < 3)
                    continue;
                bool valid = true;
                for (const integer id : face.points) {
                    if (id < 0 or static_cast<std::size_t>(id) >= shape.size())
                        valid = false;
                }
                if (not valid)
                    continue;
                const vec3 origin = shape[static_cast<std::size_t>(face.points[0])];
                vec3 normal = face.normal;
                if (glm::dot(normal, normal) <= hullEps * hullEps)
                    normal = glm::cross(shape[static_cast<std::size_t>(face.points[1])] - origin, shape[static_cast<std::size_t>(face.points[2])] - origin);
                const float normalLen = glm::length(normal);
                if (normalLen <= hullEps)
                    continue;
                normal /= normalLen;
                vec3 tangent{0.0f, 0.0f, 0.0f};
                for (std::size_t index = 1; index < face.points.size(); ++index) {
                    tangent = shape[static_cast<std::size_t>(face.points[index])] - origin;
                    tangent -= normal * glm::dot(tangent, normal);
                    if (glm::dot(tangent, tangent) > hullEps * hullEps)
                        break;
                }
                if (glm::dot(tangent, tangent) <= hullEps * hullEps)
                    continue;
                tangent = glm::normalize(tangent);
                const vec3 bitangent = glm::cross(normal, tangent);
                const integer base = static_cast<integer>(cpu.positions.size());
                for (const integer id : face.points) {
                    const vec3 point = shape[static_cast<std::size_t>(id)];
                    const vec3 delta = point - origin;
                    cpu.positions.push_back(point);
                    cpu.normals.push_back(normal);
                    cpu.uv0.push_back(UV{glm::dot(delta, tangent), glm::dot(delta, bitangent)});
                }
                for (integer fan = 1; fan + 1 < static_cast<integer>(face.points.size()); ++fan) {
                    cpu.indices.push_back(base);
                    cpu.indices.push_back(base + fan);
                    cpu.indices.push_back(base + fan + 1);
                }
            }
        }
        return cpu;
    }

}
