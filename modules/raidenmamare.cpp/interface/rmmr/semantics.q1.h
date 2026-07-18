#pragma once

#include <rmmr/semantics/uniform.h>
#include <rmmr/semantics/geometry.h>
#include <rmmr/semantics/rendering.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Uniform {
        using Id = ::rmmr::material::Semantics::PersistentId;
        using Type = ::rmmr::material::Semantics::Type;
        using Location = ::rmmr::material::Semantics::RenderId;
        using Palette = vector<Id>;

        struct Binding {
            Id id;
            Type type;
            Location location;
        };
    };

}
