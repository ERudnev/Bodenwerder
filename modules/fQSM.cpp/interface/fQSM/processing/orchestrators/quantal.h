#pragma once

#include <concepts>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

    namespace detail {

        template<category::Any Meta>
        auto& patch_line(Writing& gate) {
            return static_cast<model::linear::Patch<Meta>&>(gate.workers_interface().updates<Meta>());
        }

    } // namespace detail

    // Mutable view of a Quantum already living in the Writing patch (no private buffer).
    // Lifetime of the referenced Quantum is the Writing/patch; this gate only keeps the Gate alive.
    // Soft noop discard: if Quantum is equality_comparable and this gate created an unchanged patchlet, drop it.
    template<category::Any Meta>
    struct QuantumGate {
        const Id<Meta> id;

        explicit QuantumGate(Writing gate, Id<Meta> id)
            : id(id)
            , gate(std::move(gate))
            , value(open_patchlet(*this))
        {}

        QuantumGate(const QuantumGate&) = delete;
        QuantumGate& operator=(const QuantumGate&) = delete;
        QuantumGate(QuantumGate&&) = delete;
        QuantumGate& operator=(QuantumGate&&) = delete;

        ~QuantumGate() {
            discard_if_noop();
        }

        Quantum<Meta>* operator->() { return &value; }
        Quantum<Meta>& operator*() { return value; }

    private:
        static Quantum<Meta>& open_patchlet(QuantumGate& self) {
            auto& patch = detail::patch_line<Meta>(self.gate);
            if (auto* patchlet = patch.items.find(self.id); patchlet and patchlet->has_value()) {
                return patchlet->value();
            }

            auto& quantum = patch.update_modification(
                self.id,
                [&]() -> const Quantum<Meta>& {
                    const auto* found = self.gate->aspect<Meta>().items().find(self.id);
                    if (!found) {
                        throw std::runtime_error(std::format(
                            R"(cannot modify "{}" {}: not present)",
                            Rtid::name<Meta>(),
                            self.id));
                    }
                    return *found;
                });
            if constexpr (std::equality_comparable<Quantum<Meta>>) {
                self.original = quantum;
            }
            return quantum;
        }

        void discard_if_noop() {
            if constexpr (std::equality_comparable<Quantum<Meta>>) {
                if (original.has_value() and value == *original) {
                    detail::patch_line<Meta>(gate).items.discard_changes(id);
                }
            }
        }

        Writing gate;
        std::optional<Quantum<Meta>> original;
        Quantum<Meta>& value;
    };

    // Mutable view of GlobalValue in the Writing patch (same immediate semantics as QuantumGate).
    template<category::Any Meta>
    struct GlobalGate {
        explicit GlobalGate(Writing gate)
            : gate(std::move(gate))
            , value(open_global(*this))
        {}

        GlobalGate(const GlobalGate&) = delete;
        GlobalGate& operator=(const GlobalGate&) = delete;
        GlobalGate(GlobalGate&&) = delete;
        GlobalGate& operator=(GlobalGate&&) = delete;

        ~GlobalGate() {
            discard_if_noop();
        }

        GlobalValue<Meta>* operator->() { return &value; }
        GlobalValue<Meta>& operator*() { return value; }

    private:
        static GlobalValue<Meta>& open_global(GlobalGate& self) {
            auto& patch = detail::patch_line<Meta>(self.gate);
            if (patch.global.has_value()) {
                return *patch.global;
            }

            auto& global = patch.update_global(
                [&]() -> const GlobalValue<Meta>& {
                    return self.gate->aspect<Meta>().global();
                });
            if constexpr (std::equality_comparable<GlobalValue<Meta>>) {
                self.original = global;
            }
            return global;
        }

        void discard_if_noop() {
            if constexpr (std::equality_comparable<GlobalValue<Meta>>) {
                if (original.has_value() and value == *original) {
                    detail::patch_line<Meta>(gate).global.reset();
                }
            }
        }

        Writing gate;
        std::optional<GlobalValue<Meta>> original;
        GlobalValue<Meta>& value;
    };

}
