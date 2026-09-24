#pragma once

#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>

namespace fqsm::processing::algorithm {

    // Applies the schema's structural rules to the changes under review, to closure: a deletion that a rule
    // writes is a removal for the rules of the next pass, until a pass deletes nothing. The deletions and group
    // unhooks go into the proposal patch itself, so the registered reactions of the same wave see the whole
    // cascade. Refusals go into the summary of refusals (the wave's pass patch). The refusal rules read the
    // additions before the closure deletes anything. A rule runs only when its listened slot has changes in
    // the proposal or is tainted.
    void apply_structural_rules(model::complex::Future& proposal, model::complex::Future& refusals, const meta::Rtid::Set& tainted);
}
