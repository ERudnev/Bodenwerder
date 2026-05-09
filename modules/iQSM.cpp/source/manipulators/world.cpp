#include <iQSM/manipulators/world.h>
#include <iQSM/state/world.h>

namespace iqsm::manipulator::world {

    World create(Schema schema) {
        return base::make_shared<state::WorldData>(schema);
    }
}