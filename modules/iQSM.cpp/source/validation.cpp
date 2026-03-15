#include <iQSM/operations/validation.h>
#include <iQSM/operations/integration.h>
#include <utility>

namespace iqsm::ops::validation {
    Delta List::apply(World world) const {
        auto current = std::move(world);
        auto summary = ::iqsm::delta::empty();

        for (const auto validator : list) {
            if (not validator) continue;
            auto delta = validator(current);
            summary = iqsm::ops::merge(std::move(summary), delta);
            current = iqsm::ops::integrate(std::move(current), std::move(delta));
        }

        return summary;
    }
}


