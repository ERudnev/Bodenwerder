#include <rmmr/resources/builders/geometryGenerator.h>

#include <cmath>

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
                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);
                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
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

        return CpuPresentation{
            .layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position"}),
            .positions = vector<Pos>{
                Pos{-half, 0.0f, -half},
                Pos{half, 0.0f, -half},
                Pos{half, 0.0f, half},
                Pos{-half, 0.0f, -half},
                Pos{half, 0.0f, half},
                Pos{-half, 0.0f, half},
            },
            .normals = {},
            .uv0 = {},
            .indices = {},
        };
    }

}
