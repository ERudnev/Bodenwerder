#pragma once

#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>

namespace fqsm::processing::algorithm {
    void integrate(model::complex::Reality&, const model::complex::Patch&);
    // The same, moving the values out of the patch: for a patch that is discarded right after (the Realm accept).
    // Keeps the inbound indexes of the Reality current; the links of a tainted slot are rebuilt from the line.
    void integrate_consuming(model::complex::Reality&, model::complex::Patch&, const meta::Rtid::Set& tainted);
}
