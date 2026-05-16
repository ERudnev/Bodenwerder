#pragma once

// Q1 language basic types (alias)
#include <fQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute):
#include <fQSM/api/aspects.h>
#include <fQSM/meta/aspect.h>
//#include <iQSM/meta/require.h>

// manipulators
//#include <iQSM/manipulators/schema.h>
//#include <iQSM/manipulators/world.h>
//#include <iQSM/manipulators/item.h>

// flow (transactions)
//#include <iQSM/flow/transactions/accumulator.h>
//#include <iQSM/flow/transactions/branch.h>
//#include <iQSM/flow/transactions/once.h>
//#include <iQSM/flow/transactions/sequence.h>
//#include <iQSM/flow/transactions/staged.h>


namespace fqsm::api {
    // Q1 language builtin types
    using namespace fqsm::q1;

    // add operations as short "ops":
    //namespace ask = ::fqsm::manipulator;

    // archetype registration:
    template<typename Meta>
    using Register = fqsm::meta::Aspect<Meta>;

    // Aspect types:
    template<typename Meta>
    using Entity = fqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Runtime>
    using Controller = fqsm::aspects::Controller<Meta, Runtime>;

    template<typename Meta, typename Parent>
    using Attribute = fqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = fqsm::aspects::Component<Meta, Parent>;

    //template<typename... Deps>
    //using Require = fqsm::meta::Require<Deps...>;

    // flow/transactions/contexts mechanism
    //namespace context {
    //    using Branch = ::fqsm::flow::Branch;
    //}
}