#include <rmmr/resources/builders/geometryGenerator.h>

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
