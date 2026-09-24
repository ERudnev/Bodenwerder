#pragma once

#include <fQSM/erased/algorithms.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/reality.h>
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
        auto& target = static_cast<model::linear::Reality<Meta>&>(world.aspect<Meta>());
        erased::integrate(target.writableLine(), patch.aspect<Meta>().line);
    }
}
