#pragma once

#include <format>
#include <memory>

#include <fQSM/references.h>
#include <fQSM/meta/categories.h>
#include <fQSM/model/_forwards.h>

#ifndef _DBG_TX_
//#define _DBG_TX_(...) ::base::message(std::format(__VA_ARGS__))
#define _DBG_TX_(...) {}
#endif

namespace fqsm::processing {
    class Session;
    struct Reading;
    struct Writing;
    struct Stewarding;
    struct Reacting;
    struct Retrospecting;
    struct SettingUp;
    struct Transaction;

    template<meta::category::Any>
    struct Direct;

    namespace orchestrator { struct Realm; struct Branch; }

    // phase 1 spellings (contract section 4), kept compiling for one release
    using View = Reading;
    using Gate = Writing;
    using Dock = Stewarding;
    template<meta::category::Any Meta> using Breach = Direct<Meta>;
}

namespace fqsm::processing::persistency {
    struct Archive;
    struct Graph;
    struct Archivist;
}

// Contexts are first-class names of fQSM. A function signature says what the function may do.
namespace fqsm {
    using Reading = processing::Reading;
    using Writing = processing::Writing;
    using Retrospecting = processing::Retrospecting;
    using Reacting = processing::Reacting;
    using Stewarding = processing::Stewarding;
    using SettingUp = processing::SettingUp;

    // Direct<T> changes the Realm state in place, outside the patch; only a Stewarding session gives it.
    template<meta::category::Any Meta>
    using Direct = processing::Direct<Meta>;
}
