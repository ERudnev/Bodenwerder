#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <fQSM/identifier.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/context.h>

namespace fqsm::manipulator::item {
    template<aspect::Standalone Meta>
    auto create(Writing, Quantum<Meta> value) -> Id<Meta>;

    template<aspect::Parasitic Meta>
    void create(Writing, Id<Meta>, Quantum<Meta>);

    template<aspect::Any Meta>
    auto get(Reading, Id<Meta>)-> const Quantum<Meta>&;

    template<aspect::Any Meta>
    auto getopt(Reading, Id<Meta>) -> std::optional<std::reference_wrapper<const Quantum<Meta>>>;
}

//
// impl
namespace fqsm::manipulator::item {

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
    auto get(Reading view, Id<Meta> id) -> const Quantum<Meta>& {
        return view.items<Meta>().at(id);
    }

    template<aspect::Any Meta>
    auto getopt(Reading view, Id<Meta> id) -> std::optional<std::reference_wrapper<const Quantum<Meta>>> {
        const auto* found = view.items<Meta>().find(id);
        if (!found) return std::nullopt;
        return cref(*found);
    }


}