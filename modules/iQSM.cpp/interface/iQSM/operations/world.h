#pragma once

#include <memory>

#include <iQSM/world.h>

namespace iqsm::ops::world {
    inline World create(Schema schema) {
        required(schema, "ops::world::create(): schema");
        return std::make_shared<const WorldObject>(std::move(schema));
    }
}

