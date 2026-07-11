#pragma once

#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

    // not derived from Transaction, because it is... "final" one, not allowed to propagate context
    // TODO: consider to remove this thing one day...

    // [deprecated]
    template<category::Any Meta>
    struct Quantal {
        const Id<Meta> id;

        explicit Quantal(Writing gate, Id<Meta> id)
            : id(id), gate(std::move(gate)), buffer(requireActual(this->gate, id)) {}

        ~Quantal() {
            gate.workers_interface().updates<Meta>().put_modification(id, std::move(buffer));
        }

        Quantal(const Quantal&) = delete;
        Quantal& operator=(const Quantal&) = delete;
        Quantal(Quantal&&) = delete;
        Quantal& operator=(Quantal&&) = delete;

        Quantum<Meta>* operator->() { return &buffer; }
        Quantum<Meta>& operator*() { return buffer; }

    protected:
        std::optional<Quantum<Meta>> getActual(Reading source, Id<Meta> itemId) const {
            return source->aspect<Meta>().items().get(itemId);
        }

        Quantum<Meta> requireActual(Reading source, Id<Meta> itemId) const {
            const auto actual = getActual(source, itemId);
            if (!actual) {
                throw std::runtime_error(std::format(
                    R"(cannot modify "{}" {}: not present)",
                    Rtid::name<Meta>(),
                    itemId));
            }
            return *actual;
        }

        Writing gate;
        Quantum<Meta> buffer;
    };

    template<category::Any Meta>
    struct Global {
        explicit Global(Writing gate)
            : gate(std::move(gate)), buffer(this->gate->aspect<Meta>().global()) {}

        ~Global() {
            gate.workers_interface().updates<Meta>().put_global(std::move(buffer));
        }

        Global(const Global&) = delete;
        Global& operator=(const Global&) = delete;
        Global(Global&&) = delete;
        Global& operator=(Global&&) = delete;

        GlobalValue<Meta>* operator->() { return &buffer; }
        GlobalValue<Meta>& operator*() { return buffer; }

    private:
        Writing gate;
        GlobalValue<Meta> buffer;
    };

}
