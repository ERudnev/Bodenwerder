#pragma once

#include <base/maybe.h>

#include <functional>
#include <utility>

#include <fQSM/identifier.h>
#include <fQSM/processing/transactions/quantal.h>
#include <fQSM/processing/commit.h>

namespace fqsm::manipulation::item {
    template<aspect::Standalone Meta>
    auto create(Writing, Quantum<Meta> value) -> Id<Meta>;

    template<aspect::Parasitic Meta>
    void create(Writing, Id<Meta>, Quantum<Meta>);

    template<aspect::Any Meta>
    auto get(Reading, Id<Meta>) -> base::maybe<std::reference_wrapper<const Quantum<Meta>>>;

    template<aspect::Any Meta>
    auto exists(Reading, Id<Meta>) -> bool;

    template<aspect::Any Meta>
    using update = processing::transaction::Quantal<Meta>;
}

//
// impl
namespace fqsm::manipulation::item {

    template<aspect::Standalone Meta>
    auto create(Writing context, Quantum<Meta> value) -> Id<Meta> {
        const auto id = Id<Meta>::generate_random();
        context.patch().template items<Meta>().insert(id, std::move(value));
        return id;
    }

    template<aspect::Parasitic Meta>
    void create(Writing context, Id<Meta> id, Quantum<Meta> value) {
        context.patch().template items<Meta>().insert(id, std::move(value));
    }

    template<aspect::Any Meta>
    auto get(Reading view, Id<Meta> id) -> base::maybe<std::reference_wrapper<const Quantum<Meta>>> {
        const auto* found = view.items<Meta>().find(id);
        if (!found) return std::nullopt;
        return std::cref(*found);
    }

    template<aspect::Any Meta>
    auto exists(Reading view, Id<Meta> id) -> bool {
        return view.items<Meta>().find(id) != nullptr;
    }

}