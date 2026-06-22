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
    struct Gate;
    struct Review;

    template<meta::category::Any>
    struct Breach;
}

// exportin this as 1st class citizen of fQSM:
namespace fqsm {
    //using Reading = const processing::View&; // cleanup
    using Reading = const ::fqsm::model::complex::State&;
    using Writing = processing::Gate; // rename to "Draft"?
    template<meta::category::Any Meta>
    using Direct = processing::Breach<Meta>;
    using Reviewing = processing::Review;
}