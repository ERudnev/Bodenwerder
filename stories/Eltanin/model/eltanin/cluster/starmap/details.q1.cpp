#include <eltanin/cluster/starmap/details.q1.h>

namespace eltanin::cluster::starmap {

    using namespace fqsm::api;

    auto doctrine::details() -> Schema {
        return ask::schema::aspect<Details>();
    }

}
