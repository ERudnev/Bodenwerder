#include <eltanin/locality/thing.q1.h>

#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    void Thing::Actions::update(Stewarding context, int64 dtUs) {
        if (dtUs <= 0)
            return;
        context.direct<Thing>().global.now += dtUs;
        with<Bullet>::update(context);
        with<Construct>::update(context);
        with<geo::Rock>::update(context);
        with<geo::Boulder>::update(context);
    }

}
