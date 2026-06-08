#pragma once

// Q1 language basic types (alias)
#include <fQSM/api/builtins.h>

// aspect types (Entity/Component/Attribute):
#include <fQSM/api/aspects.h>

// manipulation
#include <fQSM/manipulation/schema.h>
#include <fQSM/manipulation/global.h>
#include <fQSM/manipulation/item.h>

// processing (transactions, e.t.c)
#include <fQSM/processing/transactions/realm.h>
#include <fQSM/processing/transactions/branch.h>

// Codex definition for Aspects:
#include <fQSM/features/reactions/structural.h>
#include <fQSM/features/reactions/standard.h>

namespace fqsm::api {
    // Q1 language builtin types
    using namespace ::fqsm::q1;

    // add operations as short "ops":
    namespace ask = ::fqsm::manipulation;

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

    // Codex builder:
    namespace reflex {
        using namespace ::fqsm::features::reflexes;
    }
    namespace norma {
        using namespace ::fqsm::features::reactions::normas::structural;
        //namespace fqsm::features::reactions
    }
    namespace reaction {
        using namespace ::fqsm::features::reactions::standard;
    }
}