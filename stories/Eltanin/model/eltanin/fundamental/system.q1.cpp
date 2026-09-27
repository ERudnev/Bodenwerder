#include <eltanin/fundamental/system.q1.h>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    auto doctrine::system() -> Schema {
        return ask::schema::aspect<System>();
    }

}
