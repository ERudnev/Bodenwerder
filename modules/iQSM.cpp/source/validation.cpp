#include <iQSM/operations/validation.h>
#include <iQSM/operations/transaction.h>
#include <utility>

namespace iqsm::ops::validation {
    Delta List::apply(World world) const {
        auto transaction = Transaction::integrator(std::move(world));
        for (const auto validator : list) {
            if (not validator) { continue; }
            transaction.absorb(validator(transaction.world));
        }
        return transaction.summary();
    }
}


