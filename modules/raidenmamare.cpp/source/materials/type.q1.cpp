#include <Raidenmamare/materials/type.q1.h>

#include <stdexcept>

namespace rmmr::material {
    struct Type_private {
        using Runtime = Type::RuntimeStorage;

        static auto create_runtime(Reading world, const Type::Quantum& quantum, resources::Manager manager) -> Runtime {
            if (!manager->materialized<Program>(quantum.passport.program)) {
                throw std::runtime_error("Type::Materializer::materialize: program is not open");
            }

            const Program::RuntimeAccess program = Program::Operations::provide(world, quantum.passport.program, manager);
            if (!program) {
                throw std::runtime_error("Type::Materializer::materialize: program runtime is null");
            }

            return Runtime{
                .program = program,
            };
        }
    };

    void Type::Materializer::materialize(resources::Manager manager, Reading world, Id id) const {
        if (manager->materialized<Type>(id)) {
            throw std::runtime_error("Type::Materializer::materialize: resource is already materialized");
        }

        const auto& quantum = ops::particle::get<Type>(world, id);
        manager->layer<Type>().materialize(id, Type_private::create_runtime(world, quantum, manager));
    }

    void Type::Materializer::release(resources::Manager manager, Reading, Id id) const {
        auto& layer = manager->layer<Type>();
        layer.value(id).program = 0;
        layer.release(id);
    }

    const Invariants Type::invariants{
        .structural = {},
        .logical = {},
    };
}
