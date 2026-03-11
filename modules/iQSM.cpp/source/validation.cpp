#include <iQSM/operations/validation.h>
#include <iQSM/operations/transaction.h>
#include <utility>

namespace iqsm::ops::validation {
    Delta List::apply(World world) const {
        Transaction transaction = Transaction::integrator(std::move(world));
        for (const auto v : list) {
            if (not v) { continue; }
            transaction.absorb(v(transaction.current));
        }
        return transaction.summary;
    }
}


