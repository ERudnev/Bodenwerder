#include "game.h"
#include "geo/assets.h"
#include "locality/assets.h"

#include <eltanin/decorations/dust.q1.h>
#include <eltanin/cluster/celestial.q1.h>
#include <eltanin/cluster/thing.q1.h>
#include <eltanin/cluster/orbital.q1.h>
#include <eltanin/cluster/starmap/details.q1.h>
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
            cluster::doctrine::thing(),
            cluster::doctrine::orbital(),
            cluster::doctrine::celestial(),
            cluster::starmap::doctrine::details(),
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
        entities.astronomy.generate(context);
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
        if (not view.editor.addAssets(context, *shared))
            return;
        if (not view.starMap.visuals.addAssets(context))
            return;
    }

    void Game::prepareAssets(Writing) {
    }

    void Game::setup(Writing context, system::Window::Id window) {
        view.shown = Shown::map;
        ui.menuOpen = false;
        with<World>::modify_global(context)->window = window;
        view.starMap.open(context, window);
        view.starMap.bind(context, entities.astronomy);
        showMap(context);
        const auto manager = with<::rmmr::resource::Manager>::singleton(context);
        entities.blueprintPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "blueprints");
        entities.mountPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "fittings");
        view.editor.create(context);
        engageInputs(context);
    }

    void Game::onFrame(establish::Realm& world, int64 dt_us) {
        if (not entities.mountPack.ready)
            entities.mountPack.loadFromDisk(world);
        if (not entities.blueprintPack.ready) {
            entities.blueprintPack.loadFromDisk(world);
            if (entities.blueprintPack.unnamed)
                world.branch([&](Writing context) { view.editor.show(context, *entities.blueprintPack.unnamed); });
        }
        Stewarding frame = world;
        if (view.shown == Shown::editor)
            return;
        with<World>::pollPauseKey(frame);
        const seconds wallDt = static_cast<seconds>(dt_us) / 1'000'000.0;
        const seconds simDt = wallDt * World::Always::rate(with<World>::get_global(frame).warp);
        with<cluster::Thing>::update(frame, simDt);
        view.starMap.follow(frame);
        if (view.locality) {
            view.locality->tick(frame, simDt);
            if (view.shown == Shown::locality)
                view.locality->handleCameraHotkey(frame);
        }
    }

    void Game::engageInputs(Writing context) {
        const bool map = view.shown == Shown::map;
        const bool editorShown = view.shown == Shown::editor;
        const bool localityShown = view.shown == Shown::locality;
        if (view.starMap.input)
            with<system::ViewInput>::engage(context, *view.starMap.input, map);
        if (view.editor.state.mainScene.input)
            with<system::ViewInput>::engage(context, *view.editor.state.mainScene.input, editorShown and not view.editor.state.paletteMode);
        if (view.editor.state.paletteScene.input)
            with<system::ViewInput>::engage(context, *view.editor.state.paletteScene.input, editorShown and view.editor.state.paletteMode);
        if (view.locality)
            view.locality->engageCameras(context, localityShown);
    }

    void Game::showMap(Writing) {
        view.shown = Shown::map;
        if (view.starMap.view)
            views = {*view.starMap.view};
    }

    void Game::showLocality(Writing context) {
        if (view.shown == Shown::locality)
            return;
        const auto bound = with<World>::get_global(context).window;
        if (not bound)
            return;
        if (not view.locality) {
            view.locality.emplace();
            if (not view.locality->create(context, *bound, assets)) {
                view.locality.reset();
                return;
            }
        }
        view.shown = Shown::locality;
        view.locality->present(views);
    }

    void Game::showEditor(Writing) {
        view.shown = Shown::editor;
        view.editor.openPanels();
    }

}
