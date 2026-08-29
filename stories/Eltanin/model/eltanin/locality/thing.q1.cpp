#include <eltanin/locality/thing.q1.h>

#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    void Thing::Actions::update(Writing context, int64 dtUs) {
        if (dtUs <= 0)
            return;
        with<Thing>::modify_global(context)->now += dtUs;
        with<Bullet>::update(context);
        with<Construct>::update(context);
        with<Scrap>::update(context);
        with<geo::Rock>::update(context);
        with<geo::Boulder>::update(context);
    }

}
