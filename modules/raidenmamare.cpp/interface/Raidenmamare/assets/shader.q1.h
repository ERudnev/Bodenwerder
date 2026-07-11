#pragma once

#include <Raidenmamare/materials/uniformSemantics.h>
#include <Raidenmamare/resources/shader.q1.h>
#include <Raidenmamare/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::asset {

    using namespace fqsm::api;

    struct Uniform {
        using Id = material::Semantics::PersistentId;
        using Type = material::Semantics::Type;
        using Location = material::Semantics::RenderId;
        using Palette = vector<Id>;

        struct Binding {
            Id id;
            Type type;
            Location location;
        };
    };

    struct Shader : Entity<Shader> {
        struct Always {
            static auto vertexFilename(const string& name, const string& library) -> string;
            static auto fragmentFilename(const string& name, const string& library) -> string;
        };
        struct Quantum {
            string name;
            string library;
        };
        struct Actions : BaseActions {
            static auto compile(Writing, Id, system::Device::Id) -> resource::Shader::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
