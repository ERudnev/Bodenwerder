#pragma once

#include <fQSM/processing/transaction.h>

namespace fqsm::processing::transaction {

    namespace aspect = fqsm::meta::aspect;

    // Parasitic (uses other Patch) transaction, as minimal gate to transaction mechanism
    template<aspect::Any Meta>
    struct Gate : protected Transaction {
        Gate(Permit& permit) : Transaction(permit) {}

        void addOrModify(Id<Meta> id, Quantum<Meta> update) {
            context->patch->items<Meta>().insert(id, std::move(update));
        }
        
        void remove(Id<Meta> id) {
            context->patch->items<Meta>().insert(id, std::nullopt);
        }
    };
}
