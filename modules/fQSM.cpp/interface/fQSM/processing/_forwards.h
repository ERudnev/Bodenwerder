#pragma once

#include <format>
#include <memory>

//#include <base/logging.h> // required for _DBG_TX_ (make this better)
#include <fQSM/references.h>
#include <fQSM/meta/categories.h>
#include <fQSM/model/_forwards.h>

#ifndef _DBG_TX_
//#define _DBG_TX_(...) ::base::message(std::format(__VA_ARGS__))
#define _DBG_TX_(...) {}
#endif

namespace fqsm::processing::context {
    struct Operational;
    struct Direct;
}

namespace fqsm::processing {
    struct View;
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