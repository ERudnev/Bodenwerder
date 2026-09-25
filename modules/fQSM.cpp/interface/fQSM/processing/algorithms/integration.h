#pragma once

#include <fQSM/model/_forwards.h>

namespace fqsm::processing::algorithm {
    void integrate(model::complex::Reality&, const model::complex::Patch&);
    // The same, moving the values out of the patch: for a patch that is discarded right after (the Realm accept).
    void integrate_consuming(model::complex::Reality&, model::complex::Patch&);
}
