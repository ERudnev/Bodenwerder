#pragma once

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/identifier.h>
#include <iQSM/logger.h>
#include <iQSM/aspects.h>
#include <iQSM/operations/validation.h>

#include <iQSM/q1/builtins.h>

// Minimal DSL surface for generated/etalon code.
// Intentionally small to avoid pulling the whole iqsm namespace into client headers.
// USAGE: add "using namespace iqsm::dsl_gateway;" in your generated/etalon code once and feel safe
namespace iqsm::dsl_gateway {
    using namespace iqsm::q1; // adding all builtin types

    using World = iqsm::World;
    using Delta = iqsm::Delta;

    // Particte types:
    template<typename Meta>
    using Entity = iqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Parent>
    using Attribute = iqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = iqsm::aspects::Component<Meta, Parent>;

    template<typename Meta>
    using Resource = iqsm::aspects::Resource<Meta>;

    // Aspects infrastructure:
    template<typename Meta, typename BaseType = internal::id::BaseType>
    using Identifier = iqsm::Identifier<Meta, BaseType>;

    template<typename... Deps>
    using Require = iqsm::Require<Deps...>;

    struct Invariants : iqsm::ops::validation::List, iqsm::ops::validation::Structural {
    };

    using Structural = Invariants;
}