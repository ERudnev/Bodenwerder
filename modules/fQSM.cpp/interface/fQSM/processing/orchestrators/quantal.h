#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

    namespace detail {

        // optimization exception: Writing& is still forbidden (reference to std::shared_ptr)
        template<category::Any Meta>
        auto& patch_line(const Writing& gate) {
            return static_cast<model::linear::Patch<Meta>&>(gate.workers_interface().updates<Meta>());
        }

    } // namespace detail

    // Plain patch handle: ensure a modification patchlet exists, then access it by id.
    // No noop elision — unchanged values still leave a patchlet (ImGui / bind-outliving-gate).
    // Access always goes through find(id): Table storage is not reference-stable.
    template<category::Any Meta>
    struct QuantumGate {
        const Id<Meta> id;

        explicit QuantumGate(Writing gate, Id<Meta> id)
            : id(id)
            , gate(std::move(gate))
        {
            open_patchlet();
        }

        QuantumGate(const QuantumGate&) = delete;
        QuantumGate& operator=(const QuantumGate&) = delete;
        QuantumGate(QuantumGate&&) = delete;
        QuantumGate& operator=(QuantumGate&&) = delete;

        Quantum<Meta>* operator->() { return &quantum(); }
        Quantum<Meta>& operator*() { return quantum(); }

    private:
        auto& items() {
            return detail::patch_line<Meta>(gate).items;
        }

        Quantum<Meta>& quantum() {
            auto* patchlet = items().find(id);
            if (not patchlet or not patchlet->has_value()) {
                throw std::runtime_error(std::format(
                    R"(QuantumGate: patchlet missing for "{}" {})",
                    Rtid::name<Meta>(),
                    id));
            }
            return patchlet->value();
        }

        void open_patchlet() {
            auto& patch = detail::patch_line<Meta>(gate);
            if (auto* patchlet = patch.items.find(id); patchlet and patchlet->has_value()) {
                return;
            }

            patch.update_modification(
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
    };

    template<category::Any Meta>
    struct GlobalGate {
        explicit GlobalGate(Writing gate)
            : gate(std::move(gate))
        {
            open_global();
        }

        GlobalGate(const GlobalGate&) = delete;
        GlobalGate& operator=(const GlobalGate&) = delete;
        GlobalGate(GlobalGate&&) = delete;
        GlobalGate& operator=(GlobalGate&&) = delete;

        GlobalValue<Meta>* operator->() { return &global(); }
        GlobalValue<Meta>& operator*() { return global(); }

    private:
        auto& patch() {
            return detail::patch_line<Meta>(gate);
        }

        GlobalValue<Meta>& global() {
            if (not patch().global.has_value()) {
                throw std::runtime_error(std::format(
                    R"(GlobalGate: global patchlet missing for "{}")",
                    Rtid::name<Meta>()));
            }
            return *patch().global;
        }

        void open_global() {
            auto& line = patch();
            if (line.global.has_value()) {
                return;
            }

            line.update_global(
                [&]() -> const GlobalValue<Meta>& {
                    return gate->aspect<Meta>().global();
                });
        }

        Writing gate;
    };

}
