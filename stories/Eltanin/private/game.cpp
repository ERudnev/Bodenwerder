#include "game.h"
#include "geo/assets.h"
#include "locality/assets.h"

#include <eltanin/decorations/dust.q1.h>
#include <eltanin/fundamental/celestial.q1.h>
#include <eltanin/fundamental/thing.q1.h>
#include <eltanin/fundamental/system.q1.h>
#include <eltanin/geo/boulder.q1.h>
#include <eltanin/geo/rock.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/mount.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/resting.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <eltanin/resources/assets.q1.h>
#include <eltanin/resources/geometry.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/api/_interface.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/system/window.q1.h>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema Game::schema() const {
        return ask::schema::merge({
            doctrine::world(),
            fundamental::doctrine::thing(),
            fundamental::doctrine::system(),
            fundamental::doctrine::celestial(),
            phys::doctrine::body(),
            phys::rigid::doctrine::rigid(),
            phys::doctrine::resting(),
            locality::doctrine::thing(),
            locality::doctrine::flash(),
            locality::doctrine::bullet(),
            locality::doctrine::construct(),
            locality::doctrine::scrap(),
            decorations::doctrine::dust(),
            geo::doctrine::rock(),
            geo::doctrine::boulder(),
            resource::doctrine::assets(),
            mech::doctrine::blueprint(),
            mech::doctrine::mount(),
            resource::doctrine::geometry(),
        });
    }

    void Game::createCore(Writing context) {
        const auto host = with<::rmmr::resource::Assets>::singleton(context);
        with<::eltanin::resource::Assets>::extend(context, host, ::eltanin::resource::Assets::Quantum{});
    }

    void Game::addAssets(Writing context) {
        if (not shared)
            return (void)context.refuse("eltanin::Game::addAssets: shared assets missing");
        const auto core = ::eltanin::assets::addCore(context, *shared);
        if (not core)
            return;
        assets = *core;
        locality::assets::add(context);
        if (not geo::assets::add(context, *shared))
            return;
        if (not editor.addAssets(context, *shared))
            return;
        if (not starMap.visuals.addAssets(context))
            return;
    }

    void Game::prepareAssets(Writing) {
    }

    void Game::setup(Writing context, system::Window::Id window) {
        shown = Shown::map;
        with<World>::modify_global(context)->window = window;
        starMap.open(context, window);
        showMap(context);
        const auto manager = with<::rmmr::resource::Manager>::singleton(context);
        blueprintPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "blueprints");
        mountPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "fittings");
        editor.create(context);
        engageInputs(context);
    }

    void Game::onFrame(establish::Realm& world, int64 dt_us) {
        if (not mountPack.ready)
            mountPack.loadFromDisk(world);
        if (not blueprintPack.ready) {
            blueprintPack.loadFromDisk(world);
            if (blueprintPack.unnamed)
                world.branch([&](Writing context) { editor.show(context, *blueprintPack.unnamed); });
        }
        Stewarding frame = world;
        if (shown == Shown::editor)
            return;
        with<World>::pollPauseKey(frame);
        const seconds wallDt = static_cast<seconds>(dt_us) / 1'000'000.0;
        const seconds simDt = wallDt * World::Always::rate(with<World>::get_global(frame).warp);
        with<fundamental::Thing>::update(frame, simDt);
        starMap.follow(frame);
        if (locality) {
            locality->tick(frame, simDt);
            if (shown == Shown::locality)
                locality->handleCameraHotkey(frame);
        }
    }

    void Game::engageInputs(Writing context) {
        const bool map = shown == Shown::map;
        const bool editorShown = shown == Shown::editor;
        const bool localityShown = shown == Shown::locality;
        if (starMap.input)
            with<system::ViewInput>::engage(context, *starMap.input, map);
        if (editor.state.mainScene.input)
            with<system::ViewInput>::engage(context, *editor.state.mainScene.input, editorShown and not editor.state.paletteMode);
        if (editor.state.paletteScene.input)
            with<system::ViewInput>::engage(context, *editor.state.paletteScene.input, editorShown and editor.state.paletteMode);
        if (locality)
            locality->engageCameras(context, localityShown);
    }

    void Game::showMap(Writing) {
        shown = Shown::map;
        if (starMap.view)
            views = {*starMap.view};
    }

    void Game::showLocality(Writing context) {
        if (shown == Shown::locality)
            return;
        const auto bound = with<World>::get_global(context).window;
        if (not bound)
            return;
        if (not locality) {
            locality.emplace();
            if (not locality->create(context, *bound, assets)) {
                locality.reset();
                return;
            }
        }
        shown = Shown::locality;
        locality->present(views);
    }

    void Game::showEditor(Writing) {
        shown = Shown::editor;
        editor.openPanels();
    }

}
