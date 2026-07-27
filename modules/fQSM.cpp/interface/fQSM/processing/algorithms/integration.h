#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/reality.h>

// facade
namespace fqsm::processing::algorithm {
    void integrate(model::complex::Reality&, const model::complex::Patch&);
}

// implementation
namespace fqsm::processing::algorithm::details {

    template<category::Any Meta>
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        const auto& slice = patch.aspect<Meta>();
        if (slice.global.has_value()) world.aspect<Meta>().global() = slice.global.value();

        auto& target = world.aspect<Meta>().items();
        for (const auto entry : slice.items) {
            if (not entry.value.tombstone) {
                // Scarlett's Tomorrow Is Came (see docs)
                // it is best time to check: it this Change real change or replacement of Quantum of State with the same Value
                // this check costs of find() and calling PFR operator==() but saves from unnecessary reactions

                // Mortazar: Sorry, Scarlett, I will implement this tomorrow
                target.insert(entry.id, entry.value.quantum);
            }
            else
                target.erase(entry.id);
        }
    }
}
