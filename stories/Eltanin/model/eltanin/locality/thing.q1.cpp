#include <eltanin/locality/thing.q1.h>

#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    auto Thing::Always::assemble(SettingUp& setup) -> Thing::Global {
        auto world = setup.writing();
        const auto root = with<rmmr::scene::Interface>::createScene(world);
        return Global{.now = seconds{}, .timeScale = 1.0f, .scene = root};
    }

    void Thing::Actions::update(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        with<Thing>::modify_global(context)->now += dt;
        with<Bullet>::update(context);
        with<Construct>::update(context);
        with<Scrap>::update(context);
        with<geo::Rock>::update(context);
        with<geo::Boulder>::update(context);
    }

}
