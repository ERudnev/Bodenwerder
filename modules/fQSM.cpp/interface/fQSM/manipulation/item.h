#pragma once

#include <base/maybe.h>

#include <functional>
#include <utility>

#include <fQSM/identifier.h>
#include <fQSM/processing/transactions/quantal.h>
#include <fQSM/processing/contexts/operational.h>

// must disappear with cannonball refactoring and Writing ~ State& buffered writing
namespace fqsm::manipulation::item {
    template<category::Standalone Meta>
    auto create(Writing, Quantum<Meta> value) -> Id<Meta>;

    template<category::Parasitic Meta>
    void create(Writing, Id<Meta>, Quantum<Meta>);

    template<category::Any Meta>
    auto get(Reading, Id<Meta>) -> base::maybe<std::reference_wrapper<const Quantum<Meta>>>;

    template<category::Any Meta>
    auto exists(Reading, Id<Meta>) -> bool;

    template<category::Any Meta>
    using update = processing::transaction::Quantal<Meta>;
}

//
// impl
namespace fqsm::manipulation::item {

    template<category::Standalone Meta>
    auto create(Writing context, Quantum<Meta> value) -> Id<Meta> {
        const auto id = Id<Meta>::generate_random();
        context.patch().aspect<Meta>().items.insert(id, std::move(value));
        return id;
    }

    template<category::Parasitic Meta>
    void create(Writing context, Id<Meta> id, Quantum<Meta> value) {
        context.patch().aspect<Meta>().items.insert(id, std::move(value));
    }

    template<category::Any Meta>
    auto get(Reading view, Id<Meta> id) -> base::maybe<std::reference_wrapper<const Quantum<Meta>>> {
        const auto* found = view.aspect<Meta>().items().find(id);
        if (!found) return std::nullopt;
        return std::cref(*found);
    }

    template<category::Any Meta>
    auto exists(Reading view, Id<Meta> id) -> bool {
        return view.aspect<Meta>().items().find(id) != nullptr;
    }

}