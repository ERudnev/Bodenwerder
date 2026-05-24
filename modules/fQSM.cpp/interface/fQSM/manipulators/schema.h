#pragma once

#include <initializer_list>

#include <base/shared_reference.h>

#include <fQSM/meta/concepts.h>
#include <fQSM/meta/type_list.h>
#include <fQSM/schema/dag.h>
#include <fQSM/schema/details/builders.h>

namespace fqsm::manipulator::schema {

    Schema merge(std::initializer_list<Schema> parts);

    template<meta::aspect::Any Meta>
    Schema aspect();
}
// impl:
namespace fqsm::manipulator::schema {
    inline Schema merge(std::initializer_list<Schema> parts) {
        auto out = base::make_shared<fqsm::schema::Dag>();

        for (const auto& part : parts) {
            for (const auto& [type, node] : part->nodes) {
                out->nodes.emplace(type, node);
            }
        }

        return fqsm::freeze(out);
    }

    namespace detail {
        template<typename... Metas>
        auto requirements_of(meta::internals::type_list<Metas...>) -> fqsm::schema::Dag::TypeSet {
            return fqsm::schema::Dag::TypeSet{fqsm::meta::aspect::Rtid::of<Metas>()...};
        }

        template<meta::aspect::Any Meta>
        auto requirements_of() -> fqsm::schema::Dag::TypeSet {
            return requirements_of(typename meta::deps_of<Meta>::type{});
        }
    }

    template<meta::aspect::Any Meta>
    fqsm::Schema aspect() {
        auto out = base::make_shared<fqsm::schema::Dag>();
        auto node = fqsm::schema::Dag::Node{
            fqsm::meta::aspect::Name::of<Meta>(),
            detail::requirements_of<Meta>(),
            fqsm::schema::Dag::TypeSet{},
            fqsm::schema::details::binding<Meta>(),
        };

        out->nodes.emplace(fqsm::meta::aspect::Rtid::of<Meta>(), node);
        return fqsm::freeze(out);
    }
}