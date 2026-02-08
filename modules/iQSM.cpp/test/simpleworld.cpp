#include <base/testing/macros.h>

#include "model/simple.h"

namespace tests {
    void simpleworld_evolution() {
        const auto world = SimpleModel::generate();
        EXPECT_TRUE(world != nullptr);

        ADD_FAILURE("intentional failure (placeholder)");
    }
}


