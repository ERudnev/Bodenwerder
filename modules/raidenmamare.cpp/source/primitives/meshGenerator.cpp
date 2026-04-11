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
                },
                .device = device,
                .vertices = vector<vec3>{
                    vec3{-0.5f, -0.5f, 0.0f},
                    vec3{ 0.5f, -0.5f, 0.0f},
                    vec3{ 0.0f,  0.5f, 0.0f},
                },
            }
        );
        primitive::Base::Operations::bake(transaction, id, resourceManager);

        commit.push(transaction.push());
        return id;
    }

}

