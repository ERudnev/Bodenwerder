#pragma once

#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::transaction {

    // not derived from Transaction, because it is... "final" one, not allowed to propagate context
    // TODO: consider to remove this thing one day...

    // [deprecated]
    template<category::Any Meta>
    struct Quantal {
        const Id<Meta> id;

        explicit Quantal(Writing gate, Quantum<Meta> value) requires category::Standalone<Meta>
            : id(Id<Meta>::generate_random()), gate(std::move(gate)), buffer(std::move(value)) {}

        explicit Quantal(Writing gate, Id<Meta> id)
            : id(id), gate(std::move(gate)), buffer(requireActual(this->gate, id)) {}

        explicit Quantal(Writing gate, Id<Meta> id, Quantum<Meta> value) requires category::Parasitic<Meta>
            : id(id), gate(std::move(gate)), buffer(std::move(value)) {}

        ~Quantal() {
            auto& patchItems = gate.patch().aspect<Meta>().items;
            if (removed) {
                if (getActual(gate, id))
                    patchItems.insert(id, std::nullopt);
                return;
            }
            patchItems.insert(id, std::move(buffer));
        }

        Quantal(const Quantal&) = delete;
        Quantal& operator=(const Quantal&) = delete;
        Quantal(Quantal&&) = delete;
        Quantal& operator=(Quantal&&) = delete;

        Quantum<Meta>* operator->() { return &buffer; }
        Quantum<Meta>& operator*() { return buffer; }

        void remove() { removed = true; }

    protected:
        std::optional<Quantum<Meta>> getActual(Reading source, Id<Meta> itemId) const {
            return source.aspect<Meta>().items().get(itemId);
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
        bool removed = false;
    };

}
