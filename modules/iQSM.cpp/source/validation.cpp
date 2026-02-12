#include <iQSM/operations/validation.h>
#include <iQSM/operations/transaction.h>
#include <utility>

namespace iqsm::ops::validation {
    Delta List::apply(World world) const {
        Transaction tx(std::move(world));
        for (const auto v : list) {
            if (not v) { continue; }
            tx.absorb(v(tx.current));
        }
        return tx.summary;
    }
}


