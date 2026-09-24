#pragma once

#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>

namespace fqsm::model::complex { class Future; }

namespace fqsm::processing::algorithm {

    // Applies the schema's structural rules for one normalization wave. Reads the changes under review
    // from proposal, writes corrections through adjustments (the wave's pass patch), refusals into its summary.
    // A rule runs only when its listened slot has changes in the proposal patch or is tainted.
    void apply_structural_rules(const model::complex::Future& proposal, model::complex::Future& adjustments, const meta::Rtid::Set& tainted);
}
