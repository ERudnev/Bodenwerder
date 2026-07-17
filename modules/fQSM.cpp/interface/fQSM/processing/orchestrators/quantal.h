#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

    // Mutable view of a Quantum already living in the Writing patch (no private buffer).
    // Lifetime of the referenced Quantum is the Writing/patch; this gate only keeps the Gate alive.
    template<category::Any Meta>
    struct QuantumGate {
        const Id<Meta> id;

        explicit QuantumGate(Writing gate, Id<Meta> id)
            : id(id)
            , gate(std::move(gate))
            , value(open_patchlet(this->gate, this->id))
        {}

        QuantumGate(const QuantumGate&) = delete;
        QuantumGate& operator=(const QuantumGate&) = delete;
        QuantumGate(QuantumGate&&) = delete;
        QuantumGate& operator=(QuantumGate&&) = delete;

        Quantum<Meta>* operator->() { return &value; }
        Quantum<Meta>& operator*() { return value; }

    private:
        static Quantum<Meta>& open_patchlet(Writing& gate, Id<Meta> id) {
            return gate.workers_interface().updates<Meta>().update_modification(
                id,
                [&]() -> const Quantum<Meta>& {
                    const auto* found = gate->aspect<Meta>().items().find(id);
                    if (!found) {
                        throw std::runtime_error(std::format(
                            R"(cannot modify "{}" {}: not present)",
                            Rtid::name<Meta>(),
                            id));
                    }
                    return *found;
                });
        }

        Writing gate;
        Quantum<Meta>& value;
    };

    // Mutable view of GlobalValue in the Writing patch (same immediate semantics as QuantumGate).
    template<category::Any Meta>
    struct GlobalGate {
        explicit GlobalGate(Writing gate)
            : gate(std::move(gate))
            , value(open_global(this->gate))
        {}

        GlobalGate(const GlobalGate&) = delete;
        GlobalGate& operator=(const GlobalGate&) = delete;
        GlobalGate(GlobalGate&&) = delete;
        GlobalGate& operator=(GlobalGate&&) = delete;

        GlobalValue<Meta>* operator->() { return &value; }
        GlobalValue<Meta>& operator*() { return value; }

    private:
        static GlobalValue<Meta>& open_global(Writing& gate) {
            return gate.workers_interface().updates<Meta>().update_global(
                [&]() -> const GlobalValue<Meta>& {
                    return gate->aspect<Meta>().global();
                });
        }

        Writing gate;
        GlobalValue<Meta>& value;
    };

}
