#pragma once

#include <cstddef>
#include <optional>
#include <functional>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/state/world/actual.h>

namespace tests::debug {

    template<fqsm::aspect::Any Meta>
    auto has(fqsm::Reading view) -> bool {
        return view.schema->nodes.contains(fqsm::aspect::Rtid::of<Meta>());
    }

    template<fqsm::aspect::Any Meta>
    auto read(fqsm::Reading view, fqsm::Id<Meta> id) -> std::optional<std::reference_wrapper<const fqsm::Quantum<Meta>>> {
        if (!has<Meta>(view)) return std::nullopt;
        const auto* found = view.slice<Meta>().find(id);
        if (!found) return std::nullopt;
        return std::cref(*found);
    }

    template<fqsm::aspect::Any Meta>
    auto count(fqsm::Reading view) -> std::size_t {
        if (!has<Meta>(view)) return 0;

        std::size_t out = 0;
        for (const auto entry : view.slice<Meta>()) {
            ++out;
        }
        return out;
    }
}
