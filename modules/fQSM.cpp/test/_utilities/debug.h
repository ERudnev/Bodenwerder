#pragma once

#include <cstddef>
#include <optional>
#include <functional>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/model/structure/schema.h>
#include <fQSM/model/complex/state.h>

namespace tests::debug {

    template<fqsm::category::Any Meta>
    auto has(fqsm::Reading view) -> bool {
        return view.schema->accepts<Meta>();
    }

    template<fqsm::category::Any Meta>
    auto read(fqsm::Reading view, fqsm::Id<Meta> id) -> std::optional<std::reference_wrapper<const fqsm::Quantum<Meta>>> {
        if (!has<Meta>(view)) return std::nullopt;
        const auto* found = view.aspect<Meta>().find(id);
        if (!found) return std::nullopt;
        return std::cref(*found);
    }

    template<fqsm::category::Any Meta>
    auto count(fqsm::Reading view) -> std::size_t {
        if (!has<Meta>(view)) return 0;

        std::size_t out = 0;
        for (const auto entry : view.aspect<Meta>()) {
            ++out;
        }
        return out;
    }
}
