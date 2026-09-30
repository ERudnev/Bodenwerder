#include <eltanin/cluster/celestial.q1.h>

namespace eltanin::cluster {

    using namespace fqsm::api;

    auto doctrine::celestial() -> Schema {
        return ask::schema::merge({
            ask::schema::aspect<Celestial>(),
            ask::schema::aspect<Star>(),
        });
    }

}
