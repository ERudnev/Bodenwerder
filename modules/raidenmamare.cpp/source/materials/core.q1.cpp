#include <Raidenmamare/materials/core.q1.h>

#include <GL/glew.h>

#include <stdexcept>

namespace rmmr::material {
    struct Core_private {
        using Runtime = Core::RuntimeStorage;

        static auto create_runtime(Reading world, const Core::Quantum& quantum, resources::Manager manager) -> Runtime {
            if (!manager->materialized<Program>(quantum.passport.program)) {
                throw std::runtime_error("Core::Materializer::materialize: program is not open");
            }

            const Program::RuntimeAccess program = Program::Operations::provide(world, quantum.passport.program);
            if (!program) {
                throw std::runtime_error("Core::Materializer::materialize: program runtime is null");
            }

            Semantics::RuntimeMapping locations{};
            locations.reserve(quantum.passport.uniforms.size());
            vector<Semantics::Binding> bindings{};
            bindings.reserve(quantum.passport.uniforms.size());
            for (const auto persistent_id : quantum.passport.uniforms) {
                const auto semantic_name = Semantics::name_of(persistent_id);
                if (semantic_name == Semantics::Name{"_undefined"}) {
                    locations.emplace(persistent_id, GLint{-1});
                    bindings.push_back(Semantics::Binding{
                        .id = persistent_id,
                        .type = Semantics::type_of(persistent_id),
                        .location = GLint{-1},
                    });
                    continue;
                }

                const auto uniform_name = Semantics::uniform_name(semantic_name);
                const auto location = glGetUniformLocation(program, uniform_name.c_str());
                locations.emplace(persistent_id, location);
                bindings.push_back(Semantics::Binding{
                    .id = persistent_id,
                    .type = Semantics::type_of(persistent_id),
                    .location = location,
                });
            }

            return Runtime{
                .program = program,
                .locations = std::move(locations),
                .bindings = std::move(bindings),
            };
        }
    };

    auto Core::Operations::uniformIds(const vector<string>& names) -> vector<Semantics::PersistentId> {
        vector<Semantics::PersistentId> out;
        out.reserve(names.size());

        for (const auto& name : names) {
            const auto id = Semantics::id_of(name);
            if (id == Semantics::PersistentId{0}) {
                throw std::runtime_error("Core::Operations::uniformIds: unknown uniform semantic: " + name);
            }
            out.push_back(id);
        }

        return out;
    }

    auto Core::Operations::provide(Reading world, Id id) -> RuntimeAccess {
        return world->resources->layer<Core>().provide(id);
    }

    auto Core::Operations::apply(Reading world, Id id, rmmr::Device::Id) -> RuntimeAccess {
        const auto& runtime = provide(world, id);
        glUseProgram(runtime.program);
        return runtime;
    }

    void Core::Materializer::materialize(resources::Manager manager, Reading world, Id id) const {
        if (manager->materialized<Core>(id)) {
            throw std::runtime_error("Core::Materializer::materialize: resource is already materialized");
        }

        const auto& quantum = ops::particle::get<Core>(world, id);
        manager->layer<Core>().materialize(id, Core_private::create_runtime(world, quantum, manager));
    }

    void Core::Materializer::release(resources::Manager manager, Reading, Id id) const {
        manager->layer<Core>().release(id);
    }

    const Invariants Core::invariants{
        .structural = {},
        .logical = {},
    };
}
