#include <eltanin/physics/body.q1.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    auto doctrine::body() -> Schema {
        return ask::schema::aspect<Body>();
    }

}
