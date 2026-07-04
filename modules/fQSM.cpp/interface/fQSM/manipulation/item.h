#pragma once

#include <base/maybe.h>

#include <functional>
#include <utility>

#include <fQSM/identifier.h>
#include <fQSM/processing/transactions/quantal.h>
#include <fQSM/processing/contexts/operational.h>

// must disappear with cannonball refactoring and Writing ~ State& buffered writing
namespace fqsm::manipulation::item {
    template<category::Any Meta>
    auto get(Reading, Id<Meta>) -> base::maybe<std::reference_wrapper<const Quantum<Meta>>>;

    template<category::Any Meta>
    auto exists(Reading, Id<Meta>) -> bool;

    template<category::Any Meta>
    using update = processing::transaction::Quantal<Meta>;
}

/*namespace fqsm::manipulation::quantum {

    template<category::Any Meta>
    Quantum<Meta>& get(Writing context, Id<Meta> id) {
        auto& patchItems = context.patch().aspect<Meta>().items;
        if (auto* patchlet = patchItems.find(id); patchlet == nullptr or not patchlet->has_value())
            patchItems.insert(id, context->aspect<Meta>().items().at(id));
        return patchItems.at(id).value();
    }

    template<category::Any Meta>
    const Quantum<Meta>& get(Reading context, Id<Meta> id) {
        return context.aspect<Meta>().items().at(id);
    }

    template<category::Any Meta>
    auto find(Reading context, Id<Meta> id)
        -> base::maybe<std::reference_wrapper<const Quantum<Meta>>> {
        if (const auto* found = context.aspect<Meta>().items().find(id))
            return std::cref(*found);
        return std::nullopt;
    }

    template<category::Any Meta>
    auto change(Writing context, Id<Meta> id)
        -> base::maybe<std::reference_wrapper<Quantum<Meta>>> {
        auto& patchItems = context.patch().aspect<Meta>().items;

        if (auto* patchlet = patchItems.find(id)) {
            if (not patchlet->has_value())
                return std::nullopt;
            return std::ref(*patchlet);
        }

        if (const auto* worldItem = context->aspect<Meta>().items().find(id)) {
            patchItems.insert(id, *worldItem);
            return std::ref(patchItems.at(id).value());
        }

        return std::nullopt;
    }
}*/

//
// impl
namespace fqsm::manipulation::item {

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