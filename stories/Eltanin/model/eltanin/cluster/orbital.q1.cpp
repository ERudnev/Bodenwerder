#include <eltanin/cluster/orbital.q1.h>

namespace eltanin::cluster {

    using namespace fqsm::api;

    auto doctrine::orbital() -> Schema {
        return ask::schema::merge({
            ask::schema::aspect<Axis>(),
            ask::schema::aspect<Orbit>(),
        });
    }

}
