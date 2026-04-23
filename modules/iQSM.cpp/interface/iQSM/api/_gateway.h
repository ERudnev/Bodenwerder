#pragma once

// core handles:
#include <iQSM/_forwards.h>

// builtin basic types (alias)
#include <iQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute/Handle):
#include <iQSM/aspects.h>

// typed facade (Id/Quantum/Item):
#include <iQSM/meta/facade.h>

// operations interface (validation invariants types):
#include <iQSM/operations/interface.h>
#include <iQSM/operations/validators.h>

// helpers:
#include <iQSM/helpers/particle.h>
#include <iQSM/helpers/global.h>
#include <iQSM/helpers/schema.h>
#include <iQSM/helpers/world.h>
#include <iQSM/helpers/resource.h>

#include <iQSM/repository/transactions/accumulator.h>
#include <iQSM/repository/transactions/branch.h>
#include <iQSM/repository/transactions/once.h>
#include <iQSM/repository/transactions/sequence.h>
#include <iQSM/repository/transactions/staged.h>

// resource system
#include <iQSM/resources/manager.h>
#include <iQSM/resources/materializer.h>

// Minimal DSL surface for generated/etalon code.
// Intentionally small to avoid pulling the whole iqsm namespace into client headers.
// USAGE: add "using namespace iqsm::q1_gateway;" in your generated/etalon code once and feel safe
namespace iqsm::q1_gateway {
    using namespace iqsm::q1; // adding all builtin types

    namespace ops = ::iqsm::helpers;
    namespace validator = ::iqsm::operations::validation;

    // Particte types:
    template<typename Meta>
    using Entity = iqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Parent>
    using Attribute = iqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = iqsm::aspects::Component<Meta, Parent>;

    template<typename Meta, typename RuntimeStorageType, typename RuntimeAccessType>
    using Handle = iqsm::aspects::Handle<Meta, RuntimeStorageType, RuntimeAccessType>;

    // Aspects infrastructure:
    template<typename Meta, typename BaseType = internal::id::BaseType>
    using Identifier = iqsm::Identifier<Meta, BaseType>;

    template<typename... Deps>
    using Require = iqsm::Require<Deps...>;

    template<typename Meta>
    using Item = ::iqsm::Item<Meta>;

    // experimental, try to use this a bit...
    template<typename Aspect>
    using call = Aspect::Operations;  // call<Dispatcher>::foo(...)

    // Transactions mechanism:
    using Reading = ::iqsm::Reading;
    using Writing = ::iqsm::Writing;
    namespace repo {
        using Accumulator = ::iqsm::repo::Accumulator;
        using Branch = ::iqsm::repo::Branch;
        using Sequence = ::iqsm::repo::Sequence;
        using Staged = ::iqsm::repo::Staged;
    }

    using Invariants = ::iqsm::detail::validation::Block;

    namespace invariant {
        using namespace validator::structural;
        using namespace validator::logic;
        using namespace validator::helpers;
    }

    // TODO: remove or rebind on iqsm::helpers::resource::Types
    namespace resources {
        using Provider = cref<iqsm::resources::ManagerCore>;
        using Manager = ref<iqsm::resources::ManagerCore>;
    }
}