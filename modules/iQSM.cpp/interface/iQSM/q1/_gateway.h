#pragma once

#include <iQSM/aspects.h>
#include <iQSM/identifier.h>
#include <iQSM/logger.h>
#include <iQSM/particles.h>
#include <iQSM/operations/validation.h>

#include <iQSM/q1/builtins.h>

// Minimal DSL surface for generated/etalon code.
// Intentionally small to avoid pulling the whole iqsm namespace into client headers.
// USAGE: add "using namespace iqsm::dsl_gateway;" in your generated/etalon code once and feel safe
namespace iqsm::dsl_gateway {
    using namespace iqsm::q1; // adding all builtin types

    // Particte types:
    template<typename Meta>
    using Xion = iqsm::particles::Xion<Meta>;

    template<typename Meta, typename Parent>
    using Quark = iqsm::particles::Quark<Meta, Parent>;

    // Aspects infrastructure:
    template<typename Meta, typename BaseType = internal::id::BaseType>
    using Identifier = iqsm::Identifier<Meta, BaseType>;

    template<typename... Deps>
    using DependsFrom = iqsm::DependsFrom<Deps...>;

    struct Structural : iqsm::ops::validation::List, iqsm::ops::validation::Structural {
    };
}