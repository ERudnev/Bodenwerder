#pragma once

#include <rmmr/resources/texture.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::asset {

    using namespace fqsm::api;

    struct Texture : Entity<Texture> {
        struct Always {
            static auto filename(const string& name, const string& library) -> string;
        };
        struct Quantum {
            string name;
            string library;
        };
        struct Actions : BaseActions {
            static auto compile(Writing, Id, system::Device::Id) -> resource::Texture::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
