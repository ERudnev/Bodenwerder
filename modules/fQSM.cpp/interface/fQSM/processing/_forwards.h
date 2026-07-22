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
    struct Retrospective;
    struct Synchronous;
}

namespace fqsm::processing {
    struct View;
    struct Gate;
    struct Wall;
    struct Dock;
    struct Review;

    template<meta::category::Any>
    struct Breach;
}

namespace fqsm::processing::persistency {
    struct Archive;
    struct Graph;
    struct Archivist;
}

// exporting this as 1st class citizen of fQSM:
namespace fqsm {
    using Reading = processing::View;
    using Writing = processing::Gate;
    using Retrospecting = processing::Wall;
    using Reacting = processing::Review;
    using Stewarding = processing::Dock;

    // being aside of verbs, this is a legal tansaction workaround
    // you can manipulate State direclty with Direct<T>. May be you need it. But be aware
    template<meta::category::Any Meta>
    using Direct = processing::Breach<Meta>;
}