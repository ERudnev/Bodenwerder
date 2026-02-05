#pragma once

#include <iQSM/identifier.h>
#include <iQSM/particles.h>
#include <iQSM/q1builtins.h>

// Minimal DSL surface for generated/etalon code.
// Intentionally small to avoid pulling the whole iqsm namespace into client headers.
// USAGE: add "using namespace iqsm::dsl_gateway;" in your generated/etalon code once and feel safe
namespace iqsm::dsl_gateway {
    template<typename Meta>
    using Xion = iqsm::particles::Xion<Meta>;

    template<typename Meta, typename Parent>
    using Quark = iqsm::particles::Quark<Meta, Parent>;

    template<typename Meta, typename BaseType = internal::id::BaseType>
    using Identifier = iqsm::Identifier<Meta, BaseType>;

    using Seconds = iqsm::q1::Seconds;
    using Time = iqsm::q1::Time;
}