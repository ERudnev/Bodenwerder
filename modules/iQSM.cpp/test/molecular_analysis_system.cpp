#include <base/testing/macros.h>

// engine:
#include <iQSM/_all.include.h>

#include <Atomic/model.q1.h>
#include <Atomic/system.q1.h>
#include <iQSM/schema.h>


// test System
namespace {

    using namespace iqsm;
    using namespace Q1CORE::Example::Model;
    using namespace Q1CORE::Example::System;

    // it is prototype: testing System which
    // a) adds onw parts of global Schema as "i need this" addon
    // b) works with own data
    // c) adds own constraints to the "common" parts of global Schema
    /*struct TestSystem {
        static TestSystem build(Schema general);
        const auto schema = SchemaObject::assemble<MolecularStatistics>();

        // own logic invariants (prototype examples):
        // constrain all Positions in 1x1x1 box
        static Delta positionsClamped(World);     

    };*/

    //struct 
}


namespace tests {
    void molecular_analysis_system() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;
        using namespace Q1CORE::Example::System;

        //auto initial_schema = SchemaObject::assemble<Molecule>();
    }
}


