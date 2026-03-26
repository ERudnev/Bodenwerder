#pragma once

#include <memory>

#include <iQSM/world.h>

namespace iqsm::helpers::world {
    inline World create(Schema schema) {
        return base::make_shared<const WorldObject>(std::move(schema));
    }
}

