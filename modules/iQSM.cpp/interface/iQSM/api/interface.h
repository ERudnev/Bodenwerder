#pragma once

// Q1 language basic types (alias)
#include <iQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute/Agent):
#include <iQSM/aspects.h>
#include <iQSM/meta/registered.h>
#include <iQSM/meta/require.h>

// manipulators
#include <iQSM/manipulators/schema.h>
#include <iQSM/manipulators/world.h>
#include <iQSM/manipulators/item.h>

// flow (transactions)
//#include <iQSM/flow/transactions/accumulator.h>
#include <iQSM/flow/transactions/branch.h>
//#include <iQSM/flow/transactions/once.h>
//#include <iQSM/flow/transactions/sequence.h>
//#include <iQSM/flow/transactions/staged.h>


namespace iqsm::interface {
    // Q1 language builtin types
    using namespace iqsm::q1;

    // add operations as short "ops":
    namespace ask = ::iqsm::manipulator;

    // iqsm axis mechanism:
    using Layer = ::iqsm::state::axis::versioning;

    // Aspect types:
    template<typename Meta>
    using Entity = iqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Parent>
    using Attribute = iqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = iqsm::aspects::Component<Meta, Parent>;

    template<typename Meta, typename Runtime>
    using Agent = iqsm::aspects::Agent<Meta, Runtime>;

    template<typename... Deps>
    using Require = iqsm::meta::Require<Deps...>;


    template<typename Meta, state::axis::versioning Versioning>
    using Register = iqsm::meta::Registered<Meta, Versioning>;

    // flow/transactions mechanism
    //using Reading = 
    namespace context {
        using Branch = ::iqsm::flow::Branch;
    }
}