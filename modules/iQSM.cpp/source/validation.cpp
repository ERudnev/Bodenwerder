#include <iQSM/operations/validation.h>
#include <iQSM/operations/integration.h>
#include <iQSM/internals/fields_mutable.h>
#include <utility>

namespace iqsm::ops::validation {
    Delta List::apply(World world) const {
        auto current = std::move(world);
        internals::FieldsMutable summary{};

        for (const auto validator : list) {
            if (not validator) continue;
            repo::Commit commit{
                current,
                [&](Delta delta) {
                    summary.absorb(delta);
                    current = iqsm::ops::integrate(std::move(current), std::move(delta));
                }
            };
            validator(std::move(commit));
        }

        return summary.push();
    }
}


