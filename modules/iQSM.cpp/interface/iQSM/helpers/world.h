#pragma once

#include <memory>

#include <iQSM/world.h>

namespace iqsm::helpers::world {

    inline World create(Schema schema, resources::Provider resources) {
        return base::make_shared<const WorldObject>(std::move(schema), std::move(resources));
    }

    World create_no_resources(Schema schema);
}

