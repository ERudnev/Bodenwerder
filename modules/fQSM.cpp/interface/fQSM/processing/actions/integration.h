#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/reality.h>

// facade
namespace fqsm::processing::actions {
    void integrate(model::complex::Reality&, const model::complex::Patch&);
}

// implementation
namespace fqsm::processing::actions::details {

    template<aspect::Any Meta>
    void integrate(model::complex::Reality& world, const model::complex::Patch& patch) {
        const auto& slice = patch.aspect<Meta>();
        if (slice.global.has_value()) world.aspect<Meta>().global() = slice.global.value();

        auto& target = world.aspect<Meta>().items();
        for (const auto entry : slice.items) {
            if (entry.value.has_value()) target.insert(entry.key, entry.value.value());
            else target.erase(entry.key);
        }
    }
}
