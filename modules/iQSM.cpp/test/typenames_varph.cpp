#include <base/testing/macros.h>

#include <Atomic/varph.q1.h>
#include <iQSM/aspects.h>

namespace tests {
    void typenames_varph() {
        using namespace Q1CORE::Example::Varph;
        EXPECT_EQ(iqsm::Aspect<Spark>::typeName, "Q1CORE::Example::Varph::Spark");
        EXPECT_EQ(iqsm::Aspect<Electron>::typeName, "Q1CORE::Example::Varph::Electron");
        EXPECT_EQ(iqsm::Aspect<Atom>::typeName, "Q1CORE::Example::Varph::Atom");
    }
}

