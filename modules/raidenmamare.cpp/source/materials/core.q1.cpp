#include <Raidenmamare/materials/core.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <utility>

namespace rmmr::material {
    struct Core::Internals : Core::DefaultInternals {
        static auto create_compiled(Reading context, const Quantum& quantum) -> Compiled {
            const auto& program = with<Program>::get(context, quantum.program);
            if (!program.handle) {
                throw std::runtime_error("Core::Actions::compile: program is not compiled");
            }

            Compiled::Locations locations{};
            locations.reserve(quantum.uniforms.size());
            vector<Uniform::Binding> bindings{};
            bindings.reserve(quantum.uniforms.size());

            for (const auto uniform_id : quantum.uniforms) {
                const auto semantic_name = Semantics::name_of(uniform_id);
                if (semantic_name == Semantics::Name{"_undefined"}) {
                    locations.emplace(uniform_id, GLint{-1});
                    bindings.push_back(Uniform::Binding{
                        .id = uniform_id,
                        .type = Semantics::type_of(uniform_id),
                        .location = GLint{-1},
                    });
                    continue;
                }

                const auto uniform_name = Semantics::uniform_name(semantic_name);
                const auto location = glGetUniformLocation(program.handle, uniform_name.c_str());
                locations.emplace(uniform_id, location);
                bindings.push_back(Uniform::Binding{
                    .id = uniform_id,
                    .type = Semantics::type_of(uniform_id),
                    .location = location,
                });
            }

            return Compiled{
                .program = program.handle,
                .locations = std::move(locations),
                .bindings = std::move(bindings),
            };
        }
    };

    auto Core::Actions::uniformIds(vector<string> names) -> Uniform::Palette {
        Uniform::Palette out;
        out.reserve(names.size());

        for (const auto& name : names) {
            const auto id = Semantics::id_of(name);
            if (id == Semantics::PersistentId{0}) {
                throw std::runtime_error("Core::Actions::uniformIds: unknown uniform semantic: " + name);
            }
            out.push_back(id);
        }

        return out;
    }

    void Core::Actions::compile(Writing context, Id id, Window::Id window) {
        const auto& quantum = get(context, id);
        if (quantum.compiled) {
            throw std::runtime_error("Core::Actions::compile: material is already compiled");
        }

        const auto& program = with<Program>::get(context, quantum.program);
        if (!program.handle) {
            with<Program>::compile(context, quantum.program, window);
        }

        glfwMakeContextCurrent(with<Window>::get(context, window).handle);
        modify(context, id)->compiled = Internals::create_compiled(context, get(context, id));
    }

    void Core::Actions::apply(Reading context, Id id, Window::Id window) {
        const auto compiled = provide(context, id);
        if (!compiled) {
            throw std::runtime_error("Core::Actions::apply: material is not compiled");
        }

        glfwMakeContextCurrent(with<Window>::get(context, window).handle);
        glUseProgram(compiled->program);
    }

    auto Core::Actions::provide(Reading context, Id id) -> optional<Compiled> {
        return get(context, id).compiled;
    }
}
