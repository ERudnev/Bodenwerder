#pragma once

#include <fQSM/identifier.h>
#include <fQSM/processing/transactions/quantal.h>
#include <fQSM/processing/contexts/operational.h>

// must disappear with cannonball refactoring and Writing ~ State& buffered writing
namespace fqsm::manipulation::item {
    template<category::Any Meta>
    auto exists(Reading, Id<Meta>) -> bool;

    template<category::Any Meta>
    using update = processing::transaction::Quantal<Meta>;
}

//
// impl
namespace fqsm::manipulation::item {

    template<category::Any Meta>
    auto exists(Reading view, Id<Meta> id) -> bool {
        return view.aspect<Meta>().items().find(id) != nullptr;
    }

}
