#include <eltanin/physics/resting.q1.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    auto doctrine::resting() -> Schema {
        return ask::schema::aspect<Resting>();
    }

}
