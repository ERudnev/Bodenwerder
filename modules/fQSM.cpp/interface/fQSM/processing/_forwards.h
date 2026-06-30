#pragma once

#include <memory>

#include <fQSM/references.h>
#include <fQSM/meta/categories.h>
#include <fQSM/model/_forwards.h>

//#include <fQSM/state/world.h>

namespace fqsm::processing::context {
    struct Operational;
    struct Direct;
}

namespace fqsm::processing {
    using View = const model::complex::State&;
    struct Gate;
    struct Review;

    template<meta::category::Any>
    struct Breach;
}

// exportin this as 1st class citizen of fQSM:
namespace fqsm {
    //using Reading = const processing::View&; // cleanup
    using Reading = processing::View;
    using Writing = processing::Gate; // rename to "Draft"?
    using Reacting = processing::Review;
    template<meta::category::Any Meta>
    using Direct = processing::Breach<Meta>;

}