#include <eltanin/fundamental/celestial.q1.h>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    auto doctrine::celestial() -> Schema {
        return ask::schema::aspect<Celestial>();
    }

}
