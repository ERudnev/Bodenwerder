#pragma once

// Light header: Retrospection trait only.
// Kept separate from aspect/persistency.h so meta/categories can check the concept
// without pulling Field/Collection (and without meta → heavy aspect includes).

namespace fqsm::aspect {

    // Persist / layout form for T. Specialize beside the type (domains) or in fQSM (builtins).
    // Primary is incomplete — missing specialization fails at use.
    template<typename T>
    struct Retrospection;

    template<typename T, typename Desc>
    void describe(Desc& d) {
        Retrospection<T>::describe(d);
    }

}
