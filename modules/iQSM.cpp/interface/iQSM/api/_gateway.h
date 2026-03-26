#pragma once

// core handles:
#include <iQSM/_forwards.h>

// builtin basic types (alias)
#include <iQSM/api/builtins.h>

// aspect tags (Entity/Component/Attribute/Resource):
#include <iQSM/aspects.h>

// typed facade (Id/Quantum/Item):
#include <iQSM/meta/facade.h>

// operations interface (validation invariants types):
#include <iQSM/operations/interface.h>
#include <iQSM/operations/validators.h>

// helpers:
#include <iQSM/helpers/particle.h>
#include <iQSM/helpers/global.h>
#include <iQSM/helpers/resource.h>
#include <iQSM/helpers/schema.h>
#include <iQSM/helpers/world.h>

// repository:
#include <iQSM/repository/commit.h>
#include <iQSM/repository/branch.h>
#include <iQSM/repository/sequence.h>
#include <iQSM/repository/accumulator.h>

// Minimal DSL surface for generated/etalon code.
// Intentionally small to avoid pulling the whole iqsm namespace into client headers.
// USAGE: add "using namespace iqsm::dsl_gateway;" in your generated/etalon code once and feel safe
namespace iqsm::dsl_gateway {
    using namespace iqsm::q1; // adding all builtin types

    namespace ops = ::iqsm::helpers;
    namespace repo = ::iqsm::repo;
    namespace validator = ::iqsm::ops::validation;

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

    template<typename Meta>
    using Item = ::iqsm::Item<Meta>;

    using Invariants = ::iqsm::detail::validation::Block;

    namespace invariant {
        using namespace validator::structural;
        using namespace validator::logic;
        using namespace validator::helpers;
    }
}