#include <rmmr/scene/gizmos.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto doctrine::gizmos() -> Schema {
        return ask::schema::aspect<Grid>();
    }

}
