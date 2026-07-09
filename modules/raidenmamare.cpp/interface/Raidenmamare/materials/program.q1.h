#pragma once

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/materials/uniformSemantics.h>

#include <fQSM/api/interface.h>

namespace rmmr::material {

    using namespace fqsm::api;

    struct Uniform {
        using Id = Semantics::PersistentId;
        using Type = Semantics::Type;
        using Location = Semantics::RenderId;
        using Palette = vector<Id>;

        struct Binding {
            Id id;
            Type type;
            Location location;
        };
    };

    struct Program : Entity<Program> {
        using Handle = GLuint;

        struct Quantum {
            string name;
            string library;
            Handle handle;
        };
        struct Global {};
        struct Actions : BaseActions {
            static auto vertexFilename(string name, string library) -> string;
            static auto fragmentFilename(string name, string library) -> string;
            static void compile(Writing, Id, Window::Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Program_group : Group<Program_group, Window, Program> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };
}
