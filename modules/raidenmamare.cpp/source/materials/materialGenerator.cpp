#include "materialGenerator.h"

#include <Raidenmamare/materials/program.q1.h>

namespace rmmr::material {

    auto MaterialGenerator::ambient(Writing commit, rmmr::Device::Id device, resources::Manager resourceManager) -> Core::Id {
        repo::Sequence transaction{commit.initial};

        const material::Program::Id program = ops::resource::declare<material::Program>(
            transaction,
            material::Program::Quantum{
                .passport = material::Program::Materializer::Passport{
                    .debugName = "ambient",
                    .vertexFilename = "shaders/triangle.vert.glsl",
                    .fragmentFilename = "shaders/triangle.frag.glsl",
                },
                .device = device,
            }
        );

        material::Program::Operations::materialize(transaction, program, resourceManager);

        const auto core = ops::resource::declare<material::Core>(
            transaction,
            material::Core::Quantum{
                .passport = material::Core::Materializer::Passport{
                    .program = program,
                    // triangle.vert/frag: no lights — only transforms, per-object albedo, scene ambient hemispheres.
                    .uniforms = material::Core::Operations::uniformIds(vector<string>{
                        "model",
                        "view",
                        "projection",
                        "albedo",
                        "ambientColor",
                        "ambientIntensity",
                    }),
                },
                .name = "ambient",
            }
        );

        ops::resource::materialize<material::Core>(transaction, resourceManager, core);

        commit.push(transaction.push());
        return core;
    }

    auto MaterialGenerator::lit(Writing commit, rmmr::Device::Id device, resources::Manager resourceManager) -> Core::Id {
        repo::Sequence transaction{commit.initial};

        const material::Program::Id program = ops::resource::declare<material::Program>(
            transaction,
            material::Program::Quantum{
                .passport = material::Program::Materializer::Passport{
                    .debugName = "kube",
                    .vertexFilename = "shaders/kube.vert.glsl",
                    .fragmentFilename = "shaders/kube.frag.glsl",
                },
                .device = device,
            }
        );

        material::Program::Operations::materialize(transaction, program, resourceManager);

        const auto core = ops::resource::declare<material::Core>(
            transaction,
            material::Core::Quantum{
                .passport = material::Core::Materializer::Passport{
                    .program = program,
                    // kube.vert/frag: same basis as ambient, plus one world-space lamp (N·L + attenuation-style terms in shader).
                    .uniforms = material::Core::Operations::uniformIds(vector<string>{
                        "model",
                        "view",
                        "projection",
                        "albedo",
                        "ambientColor",
                        "ambientIntensity",
                        "light0Pos",
                        "light0Color",
                        "light0Intensity",
                    }),
                },
                .name = "lit",
            }
        );

        ops::resource::materialize<material::Core>(transaction, resourceManager, core);

        commit.push(transaction.push());
        return core;
    }

}
