#include "meshGenerator.h"

#include <stdexcept>

namespace rmmr::primitive {

    auto MeshGenerator::triangle(Writing commit, rmmr::Device::Id device, resources::Manager resourceManager) -> Base::Id {
        repo::Sequence transaction{commit.initial};

        const auto id = ops::resource::declare<Base>(
            transaction,
            Base::Quantum{
                .passport = Base::Materializer::Passport{
                    .debugName = "triangle",
                    .layout = vector<GeometrySemantics::PersistentId>{
                        GeometrySemantics::id_of("position"),
                    },
                },
                .device = device,
                .positions = vector<Pos>{
                    Pos{-0.5f, -0.5f, 0.0f},
                    Pos{ 0.5f, -0.5f, 0.0f},
                    Pos{ 0.0f,  0.5f, 0.0f},
                },
                .normals = vector<Pos>{},
            }
        );
        primitive::Base::Operations::bake(transaction, id, resourceManager);

        const auto& tri_runtime = resourceManager->layer<Base>().provide(id);
        if (!tri_runtime.vao || !tri_runtime.vbo || tri_runtime.vertex_count <= integer{0})
            throw std::runtime_error("MeshGenerator::triangle: primitive runtime is incomplete (VAO/VBO/count)");

        commit.push(transaction.push());
        return id;
    }

    auto MeshGenerator::kube(Writing commit, rmmr::Device::Id device, resources::Manager resourceManager) -> Base::Id {
        repo::Sequence transaction{commit.initial};

        const auto id = ops::resource::declare<Base>(
            transaction,
            Base::Quantum{
                .passport = Base::Materializer::Passport{
                    .debugName = "kube",
                    .layout = vector<GeometrySemantics::PersistentId>{
                        GeometrySemantics::id_of("position"),
                        GeometrySemantics::id_of("normal"),
                    },
                },
                .device = device,
                .positions = vector<Pos>{
                    // +Z
                    Pos{-0.5f, -0.5f, 0.5f},
                    Pos{0.5f, -0.5f, 0.5f},
                    Pos{0.5f, 0.5f, 0.5f},
                    Pos{-0.5f, -0.5f, 0.5f},
                    Pos{0.5f, 0.5f, 0.5f},
                    Pos{-0.5f, 0.5f, 0.5f},
                    // -Z
                    Pos{-0.5f, -0.5f, -0.5f},
                    Pos{-0.5f, 0.5f, -0.5f},
                    Pos{0.5f, 0.5f, -0.5f},
                    Pos{-0.5f, -0.5f, -0.5f},
                    Pos{0.5f, 0.5f, -0.5f},
                    Pos{0.5f, -0.5f, -0.5f},
                    // +X
                    Pos{0.5f, -0.5f, -0.5f},
                    Pos{0.5f, 0.5f, -0.5f},
                    Pos{0.5f, 0.5f, 0.5f},
                    Pos{0.5f, -0.5f, -0.5f},
                    Pos{0.5f, 0.5f, 0.5f},
                    Pos{0.5f, -0.5f, 0.5f},
                    // -X
                    Pos{-0.5f, -0.5f, -0.5f},
                    Pos{-0.5f, -0.5f, 0.5f},
                    Pos{-0.5f, 0.5f, 0.5f},
                    Pos{-0.5f, -0.5f, -0.5f},
                    Pos{-0.5f, 0.5f, 0.5f},
                    Pos{-0.5f, 0.5f, -0.5f},
                    // +Y
                    Pos{-0.5f, 0.5f, -0.5f},
                    Pos{-0.5f, 0.5f, 0.5f},
                    Pos{0.5f, 0.5f, 0.5f},
                    Pos{-0.5f, 0.5f, -0.5f},
                    Pos{0.5f, 0.5f, 0.5f},
                    Pos{0.5f, 0.5f, -0.5f},
                    // -Y
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
            }
        );
        primitive::Base::Operations::bake(transaction, id, resourceManager);

        const auto& kube_runtime = resourceManager->layer<Base>().provide(id);
        if (!kube_runtime.vao || !kube_runtime.vbo || kube_runtime.vertex_count <= integer{0})
            throw std::runtime_error("MeshGenerator::kube: primitive runtime is incomplete (VAO/VBO/count)");

        commit.push(transaction.push());
        return id;
    }

}

