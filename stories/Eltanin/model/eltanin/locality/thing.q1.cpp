#include <eltanin/locality/thing.q1.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    void Thing::Actions::update(Writing context, int64 dtUs) {
        if (dtUs <= 0)
            return;
        with<Thing>::modify_global(context)->now += dtUs;
    }

}
