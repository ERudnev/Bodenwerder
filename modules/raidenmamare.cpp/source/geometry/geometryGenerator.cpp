#include "geometryGenerator.h"

#include <stdexcept>

namespace rmmr::geometry {
    using namespace fqsm::api;
    using namespace api_for_internals;
    namespace {

        auto bake(Writing context, system::Device::Id device, asset::Geometry::Quantum asset_quantum) -> resource::Geometry::Id {
            const auto asset_geometry = with<asset::Geometry>::create(context, std::move(asset_quantum));
            const auto resource_geometry = asset::Geometry::Actions::compile(context, asset_geometry, device);

            const auto& runtime = with<resource::Geometry>::get(context, resource_geometry);
            if (not runtime.vao || not runtime.vbo || runtime.vertex_count <= integer{0}) {
                throw std::runtime_error("geometry::GeometryGenerator: geometry runtime is incomplete (VAO/VBO/count)");
            }

            return resource_geometry;
        }

    } // namespace

    auto GeometryGenerator::triangle(Writing context, system::Device::Id device) -> resource::Geometry::Id {
        return bake(context, device, asset::Geometry::Quantum{
            .debugName = "triangle",
            .layout = asset::Geometry::Always::layoutIds(vector<string>{"position"}),
            .positions = vector<Pos>{
                Pos{-0.5f, -0.5f, 0.0f},
                Pos{0.5f, -0.5f, 0.0f},
                Pos{0.0f, 0.5f, 0.0f},
            },
            .normals = {},
        });
    }

    auto GeometryGenerator::kube(Writing context, system::Device::Id device) -> resource::Geometry::Id {
        return bake(context, device, asset::Geometry::Quantum{
            .debugName = "kube",
            .layout = asset::Geometry::Always::layoutIds(vector<string>{"position", "normal"}),
            .positions = vector<Pos>{
                Pos{-0.5f, -0.5f, 0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{-0.5f, -0.5f, 0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{-0.5f, -0.5f, 0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{-0.5f, 0.5f, 0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{-0.5f, 0.5f, -0.5f},
                Pos{0.5f, 0.5f, 0.5f},
                Pos{0.5f, 0.5f, -0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{0.5f, -0.5f, -0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{-0.5f, -0.5f, -0.5f},
                Pos{0.5f, -0.5f, 0.5f},
                Pos{-0.5f, -0.5f, 0.5f},
            },
            .normals = vector<Pos>{
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, 1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{0.0f, 0.0f, -1.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{-1.0f, 0.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, 1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
                Pos{0.0f, -1.0f, 0.0f},
            },
        });
    }

    auto GeometryGenerator::gridPlane(Writing context, system::Device::Id device) -> resource::Geometry::Id {
        constexpr float half = 80.0f;

        return bake(context, device, asset::Geometry::Quantum{
            .debugName = "gridPlane",
            .layout = asset::Geometry::Always::layoutIds(vector<string>{"position"}),
            .positions = vector<Pos>{
                Pos{-half, 0.0f, -half},
                Pos{half, 0.0f, -half},
                Pos{half, 0.0f, half},
                Pos{-half, 0.0f, -half},
                Pos{half, 0.0f, half},
                Pos{-half, 0.0f, half},
            },
            .normals = {},
        });
    }

}
