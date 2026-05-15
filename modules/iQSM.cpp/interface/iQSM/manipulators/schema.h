#pragma once

#include <initializer_list>

#include <iQSM/meta/aspect.h> // registered level of meta-mechanism
#include <iQSM/meta/type_list.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/schema.h>
#include <iQSM/state/slice.h>

namespace iqsm::manipulator::schema {
    iqsm::Schema merge(std::initializer_list<iqsm::Schema> parts);

    template<aspect::Any Meta>
    iqsm::Schema aspect();
}
// impl:
namespace iqsm::manipulator::schema {
    namespace detail {
        template<typename... Metas>
        auto requirements_of(meta::internals::type_list<Metas...>) -> iqsm::state::SchemaData::TypeSet {
            return iqsm::state::SchemaData::TypeSet{iqsm::internals::Types::rttid<Metas>()...};
        }

        template<aspect::Any Meta>
        auto requirements_of() -> iqsm::state::SchemaData::TypeSet {
            return requirements_of(typename meta::deps_of<Meta>::type{});
        }
    }

    template<aspect::Any Meta>
    iqsm::Schema aspect() {
        using Versioning = typename Meta::Runtime::Versioning;
        using Aspect = iqsm::state::SchemaData::Aspect<Versioning::value>;
        //using StateSlice = iqsm::state::slice::Data<Meta, state::axis::order::state>;
        //using PatchSlice = iqsm::state::slice::Data<Meta, state::axis::order::patch>;
        constexpr auto versioning = static_cast<state::axis::versioning>(Versioning::value);
        auto out = base::make_shared<iqsm::state::SchemaData>();
        auto aspect = Aspect{
            std::string{},
            versioning,
            Aspect::template ZeroProvider<state::axis::order::state>::template create<Meta::Runtime::Slice::State>(),
            Aspect::template ZeroProvider<state::axis::order::patch>::template create<Meta::Runtime::Slice::Patch>(),
            detail::requirements_of<Meta>(),
            iqsm::state::SchemaData::TypeSet{},
        };

        if constexpr (Versioning::value == state::axis::versioning::shared) {
            out->versioned.emplace(iqsm::internals::Types::rttid<Meta>(), aspect);
        } else {
            out->operational.emplace(iqsm::internals::Types::rttid<Meta>(), aspect);
        }

        return iqsm::freeze(out);
    }
}