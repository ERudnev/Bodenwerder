#pragma once

#include <initializer_list>

#include <iQSM/meta/aspect.h> // registered level of meta-mechanism
#include <iQSM/meta/type_list.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/mechanism.h>
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
        constexpr auto versioning = static_cast<state::policy::versioning>(Versioning::value);
        auto out = base::make_shared<iqsm::state::SchemaData>();
        /* fix this code
        auto zero = [&]() -> iqsm::cref<iqsm::state::slice::Abstract> {
            if constexpr (versioning == state::policy::versioning::shared) {
                return iqsm::freeze(base::make_shared<iqsm::state::slice::Data<Meta, iqsm::state::Chunk<Meta, state::policy::order::state>>>());
            } else {
                return iqsm::freeze(base::make_shared<iqsm::state::slice::Data<Meta, iqsm::state::Chunk<Meta, state::policy::order::state>>>());
            }
        }();
        */

        out->aspects.emplace(iqsm::internals::Types::rttid<Meta>(), iqsm::state::SchemaData::Aspect{
            std::string{},
            versioning,
            zero,
            detail::requirements_of<Meta>(),
            iqsm::state::SchemaData::TypeSet{},
        });
        return iqsm::freeze(out);
    }
}