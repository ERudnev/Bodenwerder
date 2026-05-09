#pragma once

// Q1 language basic types (alias)
#include <iQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute/Agent):
#include <iQSM/aspects.h>

// operations
#include <iQSM/manipulators/schema.h>


namespace iqsm::interface {
    // Q1 language builtin types
    using namespace iqsm::q1;

    // add operations as short "ops":
    namespace ask = ::iqsm::manipulators;

    // Particle types:
    template<typename Meta>
    using Entity = iqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Parent>
    using Attribute = iqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = iqsm::aspects::Component<Meta, Parent>;

    template<typename Meta, typename Runtime>
    using Agent = iqsm::aspects::Agent<Meta, Runtime>;
}