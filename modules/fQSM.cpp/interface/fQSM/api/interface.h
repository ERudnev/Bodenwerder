#pragma once

// Q1 language basic types (alias)
#include <fQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute):
#include <fQSM/api/aspects.h>
//#include <fQSM/aspect.h>
#include <fQSM/meta/require.h>

// manipulators
#include <fQSM/manipulators/schema.h>
//#include <iQSM/manipulators/world.h>
//#include <iQSM/manipulators/item.h>

// flow (transactions)
#include <fQSM/processing/realm.h>

namespace fqsm::api {
    // Q1 language builtin types
    using namespace ::fqsm::q1;

    // add operations as short "ops":
    namespace ask = ::fqsm::manipulator;
    
    // Aspect types:
    template<typename Meta>
    using Entity = ::fqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Runtime>
    using Controller = ::fqsm::aspects::Controller<Meta, Runtime>;

    template<typename Meta, typename Parent>
    using Attribute = ::fqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = ::fqsm::aspects::Component<Meta, Parent>;

    template<typename... Deps>
    using Require = fqsm::meta::Require<Deps...>;

    using Schema = fqsm::Schema;

    // flow/transactions/contexts mechanism
    namespace context {
        using Realm = ::fqsm::processing::Realm;
    }
}