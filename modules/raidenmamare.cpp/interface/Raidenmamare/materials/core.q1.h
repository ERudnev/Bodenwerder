#pragma once

#include <Raidenmamare/materials/program.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::material {

    using namespace fqsm::api;

    struct Compiled {
        using ProgramHandle = decltype(Program::Quantum::handle);
        using Locations = Semantics::RuntimeMapping;

        ProgramHandle program;
        Locations locations;
        vector<Uniform::Binding> bindings;
    };

    struct Core : Entity<Core> {
        struct Quantum {
            string name;
            Program::Id program;
            Uniform::Palette uniforms;
            optional<Compiled> compiled;
        };
        struct Global {};
        struct Actions : BaseActions {
            static auto uniformIds(vector<string>) -> Uniform::Palette;
            static void compile(Writing, Id, Window::Id);
            static void apply(Reading, Id, Window::Id);
            static auto provide(Reading, Id) -> optional<Compiled>;
        };
        struct Internals;
        static const Behavior customAspectReactions() { return {}; }
    };
}
