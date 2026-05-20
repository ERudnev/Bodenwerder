#pragma once

#include <initializer_list>

#include <base/shared_reference.h>
#include <fQSM/aspect.h>
#include <fQSM/meta/type_list.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/schema.h>

namespace fqsm::manipulator::schema {
    Schema merge(std::initializer_list<Schema> parts);

    template<aspect::Any Meta>
    Schema aspect();
}
// impl:
namespace fqsm::manipulator::schema {
    inline Schema merge(std::initializer_list<Schema> parts) {
        auto out = base::make_shared<fqsm::state::SchemaData>();

        for (const auto& part : parts) {
            for (const auto& [type, aspect] : part->aspects) {
                out->aspects.emplace(type, aspect);
            }
        }

        return fqsm::freeze(out);
    }

    namespace detail {
        template<typename... Metas>
        auto requirements_of(meta::internals::type_list<Metas...>) -> fqsm::state::SchemaData::TypeSet {
            return fqsm::state::SchemaData::TypeSet{fqsm::meta::aspect::Rtid::of<Metas>()...};
        }

        template<aspect::Any Meta>
        auto requirements_of() -> fqsm::state::SchemaData::TypeSet {
            return requirements_of(typename meta::deps_of<Meta>::type{});
        }
    }

    template<aspect::Any Meta>
    fqsm::Schema aspect() {
        auto out = base::make_shared<fqsm::state::SchemaData>();
        auto aspect = fqsm::state::SchemaData::Aspect{
            fqsm::meta::aspect::Name::of<Meta>(),
            detail::requirements_of<Meta>(),
            fqsm::state::SchemaData::TypeSet{},
        };

        out->aspects.emplace(fqsm::meta::aspect::Rtid::of<Meta>(), aspect);
        return fqsm::freeze(out);
    }
}