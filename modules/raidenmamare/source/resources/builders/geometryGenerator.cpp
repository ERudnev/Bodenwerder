#include <rmmr/resources/builders/geometryGenerator.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <utility>

#include <glm/geometric.hpp>

namespace rmmr::resource::builders::geometry {

    using namespace fqsm::api;

    auto GeometryGenerator::triangle() -> CpuPresentation {
        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position"}),
            .positions = vector<Pos>{
                Pos{-0.5f, -0.5f, 0.0f},
                Pos{0.5f, -0.5f, 0.0f},
                Pos{0.0f, 0.5f, 0.0f},
            },
            .normals = {},
            .uv0 = {},
            .indices = {},
        };
    }

    auto GeometryGenerator::kube() -> CpuPresentation {
        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"}),
            .positions = vector<Pos>{
                Pos{-0.5f, -0.5f, 0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{-0.5f, -0.5f, 0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{-0.5f, -0.5f, 0.5f},
            },
            .normals = vector<Pos>{
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
            },
            .uv0 = vector<UV>{
                UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{1.0f, 1.0f}, UV{0.0f, 1.0f},
                UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{1.0f, 1.0f}, UV{0.0f, 1.0f},
                UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{1.0f, 1.0f}, UV{0.0f, 1.0f},
                UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{1.0f, 1.0f}, UV{0.0f, 1.0f},
                UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{1.0f, 1.0f}, UV{0.0f, 1.0f},
                UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{1.0f, 1.0f}, UV{0.0f, 1.0f},
            },
            .indices = vector<integer>{
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7,
                8, 9, 10, 8, 10, 11,
                12, 13, 14, 12, 14, 15,
                16, 17, 18, 16, 18, 19,
                20, 21, 22, 20, 22, 23,
            },
        };
    }

    // Cube with centered half-size window per face. Index order: all outer tris (6×24), then all window tris (6×6).
    auto GeometryGenerator::bagel() -> CpuPresentation {
        constexpr integer major_segments = 32;
        constexpr integer minor_segments = 16;
        constexpr float major_radius = 0.35f;
        constexpr float minor_radius = 0.15f;
        constexpr float two_pi = 6.28318530718f;
        const integer stride = minor_segments + 1;

        vector<Pos> positions;
        vector<Pos> normals;
        vector<UV> uv0;
        vector<integer> indices;
        positions.reserve((major_segments + 1) * stride);
        normals.reserve((major_segments + 1) * stride);
        uv0.reserve((major_segments + 1) * stride);
        indices.reserve(major_segments * minor_segments * 6);

        for (integer major = 0; major <= major_segments; ++major) {
            const float major_angle = two_pi * static_cast<float>(major) / static_cast<float>(major_segments);
            const float cos_major = std::cos(major_angle);
            const float sin_major = std::sin(major_angle);
            for (integer minor = 0; minor <= minor_segments; ++minor) {
                const float minor_angle = two_pi * static_cast<float>(minor) / static_cast<float>(minor_segments);
                const float cos_minor = std::cos(minor_angle);
                const float sin_minor = std::sin(minor_angle);
                positions.push_back(Pos{(major_radius + minor_radius * cos_minor) * cos_major, minor_radius * sin_minor, (major_radius + minor_radius * cos_minor) * sin_major});
                normals.push_back(Pos{cos_minor * cos_major, sin_minor, cos_minor * sin_major});
                uv0.push_back(UV{static_cast<float>(major) / static_cast<float>(major_segments), static_cast<float>(minor) / static_cast<float>(minor_segments)});
            }
        }

        for (integer major = 0; major < major_segments; ++major) {
            for (integer minor = 0; minor < minor_segments; ++minor) {
                const integer current = major * stride + minor;
                const integer next = current + stride;
                // CCW from outside (outward normals); required for GL_CULL_FACE.
                indices.push_back(current);
                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(current + 1);
                indices.push_back(next + 1);
                indices.push_back(next);
            }
        }

        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"}),
            .positions = std::move(positions),
            .normals = std::move(normals),
            .uv0 = std::move(uv0),
            .indices = std::move(indices),
        };
    }

    auto GeometryGenerator::gridPlane() -> CpuPresentation {
        constexpr float half = 80.0f;

        // CCW when viewed from +Y (front faces the sky); required for GL_CULL_FACE.
        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position"}),
            .positions = vector<Pos>{
                Pos{-half, 0.0f, -half},
                Pos{half, 0.0f, -half},
                Pos{half, 0.0f, half},
                Pos{-half, 0.0f, half},
            },
            .normals = {},
            .uv0 = {},
            .indices = {0, 2, 1, 0, 3, 2},
        };
    }

    // Unit square in XY, origin at center. Normalized UV; atlas remaps in shader later.
    // Two indexed triangles, CCW from +Z.
    auto GeometryGenerator::unitQuad() -> CpuPresentation {
        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "uv0"}),
            .positions = vector<Pos>{
                Pos{-0.5f, -0.5f, 0.0f},
                Pos{0.5f, -0.5f, 0.0f},
                Pos{0.5f, 0.5f, 0.0f},
                Pos{-0.5f, 0.5f, 0.0f},
            },
            .normals = {},
            .uv0 = vector<UV>{
                UV{0.0f, 0.0f},
                UV{1.0f, 0.0f},
                UV{1.0f, 1.0f},
                UV{0.0f, 1.0f},
            },
            .color0 = {},
            .indices = {0, 1, 2, 0, 2, 3},
        };
    }

    // Regular icosahedron, one frequency subdivision (20→80 tris). Unit sphere; smooth normals; spherical UV.
    auto GeometryGenerator::sphere() -> CpuPresentation {
        constexpr float phi = 1.61803398875f; // (1+√5)/2
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        constexpr float pi = std::numbers::pi_v<float>;

        const Pos raw[12]{
            Pos{-1.0f, phi, 0.0f}, Pos{1.0f, phi, 0.0f}, Pos{-1.0f, -phi, 0.0f}, Pos{1.0f, -phi, 0.0f},
            Pos{0.0f, -1.0f, phi}, Pos{0.0f, 1.0f, phi}, Pos{0.0f, -1.0f, -phi}, Pos{0.0f, 1.0f, -phi},
            Pos{phi, 0.0f, -1.0f}, Pos{phi, 0.0f, 1.0f}, Pos{-phi, 0.0f, -1.0f}, Pos{-phi, 0.0f, 1.0f},
        };
        vector<Pos> corners;
        corners.reserve(42);
        for (const Pos& p : raw) {
            corners.push_back(Pos{glm::normalize(glm::vec3{p})});
        }

        // Cap 5 + belt 10 + cap 5. CCW from outside (outward normals).
        vector<std::array<int, 3>> faces{
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
        };

        // One subdivision: each triangle → 4, midpoints projected onto the unit sphere.
        {
            std::map<std::pair<int, int>, int> midpoint_of;
            auto midpoint = [&](int left, int right) -> int {
                const auto key = left < right ? std::pair{left, right} : std::pair{right, left};
                if (const auto it = midpoint_of.find(key); it != midpoint_of.end()) {
                    return it->second;
                }
                const Pos mid = Pos{glm::normalize(0.5f * (glm::vec3{corners[left]} + glm::vec3{corners[right]}))};
                const int index = static_cast<int>(corners.size());
                corners.push_back(mid);
                midpoint_of.emplace(key, index);
                return index;
            };

            vector<std::array<int, 3>> subdivided;
            subdivided.reserve(faces.size() * 4);
            for (const auto& face : faces) {
                const int a = face[0];
                const int b = face[1];
                const int c = face[2];
                const int ab = midpoint(a, b);
                const int bc = midpoint(b, c);
                const int ca = midpoint(c, a);
                subdivided.push_back({a, ab, ca});
                subdivided.push_back({b, bc, ab});
                subdivided.push_back({c, ca, bc});
                subdivided.push_back({ab, bc, ca});
            }
            faces = std::move(subdivided);
        }

        auto spherical_uv = [two_pi, pi](const Pos& p) -> UV {
            const float u = 0.5f + std::atan2(p.z, p.x) / two_pi;
            const float v = 0.5f - std::asin(std::clamp(p.y, -1.0f, 1.0f)) / pi;
            return UV{u, v};
        };

        vector<Pos> positions;
        vector<Pos> normals;
        vector<UV> uv0;
        positions.reserve(faces.size() * 3);
        normals.reserve(faces.size() * 3);
        uv0.reserve(faces.size() * 3);

        for (const auto& face : faces) {
            const Pos p0 = corners[face[0]];
            const Pos p1 = corners[face[1]];
            const Pos p2 = corners[face[2]];
            UV t0 = spherical_uv(p0);
            UV t1 = spherical_uv(p1);
            UV t2 = spherical_uv(p2);

            constexpr float pole = 0.999f;
            if (std::abs(p0.y) > pole) t0.x = 0.5f * (t1.x + t2.x);
            if (std::abs(p1.y) > pole) t1.x = 0.5f * (t0.x + t2.x);
            if (std::abs(p2.y) > pole) t2.x = 0.5f * (t0.x + t1.x);

            const float min_u = std::min({t0.x, t1.x, t2.x});
            const float max_u = std::max({t0.x, t1.x, t2.x});
            if (max_u - min_u > 0.5f) {
                if (t0.x < 0.5f) t0.x += 1.0f;
                if (t1.x < 0.5f) t1.x += 1.0f;
                if (t2.x < 0.5f) t2.x += 1.0f;
            }

            const Pos tri[3]{p0, p1, p2};
            const UV uv[3]{t0, t1, t2};
            for (int corner = 0; corner < 3; ++corner) {
                positions.push_back(tri[corner]);
                normals.push_back(tri[corner]);
                uv0.push_back(uv[corner]);
            }
        }

        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"}),
            .positions = std::move(positions),
            .normals = std::move(normals),
            .uv0 = std::move(uv0),
            .color0 = {},
            .indices = {},
        };
    }

    // Regular octahedron: poles ±e_i. Split verts (per-face color0). No normals/UV.
    // Face color: R if x>0, G if y>0, B if z>0 → 8 combinatorial face colors.
    auto GeometryGenerator::diamond() -> CpuPresentation {
        vector<Pos> positions;
        vector<vec4> color0;
        vector<integer> indices;
        positions.reserve(24);
        color0.reserve(24);
        indices.reserve(24);

        for (const int sx : {1, -1}) {
            for (const int sy : {1, -1}) {
                for (const int sz : {1, -1}) {
                    Pos a{static_cast<float>(sx), 0.0f, 0.0f};
                    Pos b{0.0f, static_cast<float>(sy), 0.0f};
                    Pos c{0.0f, 0.0f, static_cast<float>(sz)};
                    // Order A,B,C is outward iff sx*sy*sz > 0; otherwise swap B/C.
                    if (sx * sy * sz < 0) {
                        std::swap(b, c);
                    }
                    const vec4 face_color{
                        sx > 0 ? 1.0f : 0.0f,
                        sy > 0 ? 1.0f : 0.0f,
                        sz > 0 ? 1.0f : 0.0f,
                        1.0f,
                    };
                    const auto base = static_cast<integer>(positions.size());
                    positions.push_back(a);
                    positions.push_back(b);
                    positions.push_back(c);
                    color0.push_back(face_color);
                    color0.push_back(face_color);
                    color0.push_back(face_color);
                    indices.push_back(base);
                    indices.push_back(base + 1);
                    indices.push_back(base + 2);
                }
            }
        }

        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "color0"}),
            .positions = std::move(positions),
            .normals = {},
            .uv0 = {},
            .color0 = std::move(color0),
            .indices = std::move(indices),
        };
    }

}
