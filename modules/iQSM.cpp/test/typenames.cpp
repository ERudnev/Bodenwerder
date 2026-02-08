#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/aspects.h>

namespace tests {
    void typenames_atomic() {
        using namespace Q1CORE::Example::Model;
        EXPECT_EQ(iqsm::Aspect<Element>::typeName, "Q1CORE::Example::Model::Element");
        EXPECT_EQ(iqsm::Aspect<Molecule>::typeName, "Q1CORE::Example::Model::Molecule");
        EXPECT_EQ(iqsm::Aspect<Atom>::typeName, "Q1CORE::Example::Model::Atom");
        EXPECT_EQ(iqsm::Aspect<Fusion>::typeName, "Q1CORE::Example::Model::Fusion");
    }
}


