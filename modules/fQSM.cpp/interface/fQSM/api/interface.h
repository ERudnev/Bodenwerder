#pragma once

// Q1 language basic types (alias)
#include <fQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute):
#include <fQSM/api/aspects.h>

// manipulation
#include <fQSM/manipulation/schema.h>
#include <fQSM/manipulation/global.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>
#include <fQSM/manipulation/_experimental.h>

// processing (transactions, e.t.c)
#include <fQSM/processing/transactions/realm.h>
#include <fQSM/processing/transactions/branch.h>

// Behavior definition for Aspects:
#include <fQSM/features/reactions/structural.h>
#include <fQSM/features/reactions/constraints.h>
#include <fQSM/features/reactions/binding.h>

namespace fqsm::api {
    // Q1 language builtin types
    using namespace ::fqsm::q1;

    // add manipulators as short "ask":
    namespace ask = ::fqsm::manipulation;
    //experimental:
    template<typename Meta>
    using with = ::fqsm::manipulation::call<Meta>;

    // Aspect types:
    template<typename Meta>
    using Entity = ::fqsm::aspects::Entity<Meta>;

    template<typename Meta, typename Runtime>
    using Controller = ::fqsm::aspects::Controller<Meta, Runtime>;

    template<typename Meta, typename Parent>
    using Attribute = ::fqsm::aspects::Attribute<Meta, Parent>;

    template<typename Meta, typename Parent>
    using Component = ::fqsm::aspects::Component<Meta, Parent>;

    using Schema = fqsm::Schema;

    using ComponentMissing = fqsm::features::reflexes::ComponentMissing;

    // processing/transactions/contexts Big Objects
    namespace context {
        using Realm = ::fqsm::processing::Realm;
        using Branch = ::fqsm::processing::transaction::Branch;
    }

    // Behavior builder:
    namespace reflex {
        using namespace ::fqsm::features::reflexes;
    }
    namespace rule {
        using namespace ::fqsm::features::reactions::rules;
        using namespace ::fqsm::features::reactions::rules;
    }
    namespace reaction {
        using namespace ::fqsm::features::reactions;
    }
}