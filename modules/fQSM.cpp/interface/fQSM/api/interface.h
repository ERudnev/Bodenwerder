#pragma once

// Q1 language basic types (alias)
#include <fQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute/Feature):
#include <fQSM/aspect/assembly.interface.h>

// manipulation
#include <fQSM/manipulation/schema.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/_experimental.h>
#include <fQSM/manipulation/_temp_sugar.h>

// processing (transactions, e.t.c)
#include <fQSM/processing/orchestrators/realm.h>
#include <fQSM/processing/orchestrators/branch.h>

// Behavior definition for Aspects:
#include <fQSM/features/behavior.h>
#include <fQSM/features/reactions/structural.h>
#include <fQSM/features/reactions/aspect_wide.h>
#include <fQSM/features/reactions/anchoring.h>
#include <fQSM/features/reactions/constraints.h>
//#include <fQSM/features/reactions/binding.h>
#include <fQSM/features/reactions/deletion.h>
#include <fQSM/features/reactions/_experimental.h>

namespace fqsm::api {
    // Q1 language builtin types
    using namespace ::fqsm::q1;

    // add manipulators as short "ask":
    namespace ask = ::fqsm::manipulation;
    //experimental:
    template<typename Meta>
    using with = ::fqsm::manipulation::call_action<Meta>;

    // Aspect types:
    template<typename Meta>
    using Entity = ::fqsm::aspect::Entity<Meta>;

    template<typename Meta, typename Parent>
    using Attribute = ::fqsm::aspect::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Feature = ::fqsm::aspect::Feature<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = ::fqsm::aspect::Component<Meta, Parent>;

    template<typename Meta, typename Host, typename Worker>
    using Group = ::fqsm::aspect::Group<Meta, Host, Worker>;

    using Behavior = ::fqsm::features::Behavior;

    // Interpretation mechanism:
    template<typename Meta>
    using Archetype = ::fqsm::aspect::Archetype<Meta>;

    template<typename Meta, meta::category::Any PrimaryType>
    using Manipulation = ::fqsm::aspect::Manipulation<Meta, PrimaryType>;

    // Types graph
    using Schema = fqsm::Schema;

    // processing/orchestrators/contexts Big Objects
    namespace establish {
        using Realm = ::fqsm::processing::orchestrator::Realm;
        using Branch = ::fqsm::processing::orchestrator::Branch;
    }

    // reactions builder:
    namespace reaction {
        using namespace ::fqsm::features::reactions;
    }
}