#include "game.h"
#include "geo/assets.h"
#include "locality/assets.h"

#include <eltanin/locality/thing.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/decorations/dust.q1.h>
#include <eltanin/geo/rock.q1.h>
#include <eltanin/geo/boulder.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <eltanin/physics/resting.q1.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/mount.q1.h>
#include <eltanin/resources/assets.q1.h>
#include <eltanin/resources/geometry.q1.h>
#include <eltanin/world.q1.h>
#include "geo/celestial/sun.h"
#include "geo/celestial/horizon.h"
#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/system/window.q1.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float spectatorFov = 60.0f * std::numbers::pi_v<float> / 180.0f;
        constexpr float orbitDistanceMin = 0.5f;
        constexpr float orbitDistanceMax = 500.0f;

        auto keyDown(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() and keys[static_cast<std::size_t>(key)];
        }

        auto bodyOfThing(Reading context, locality::Thing::Id id) -> base::maybe<phys::Body::Id> {
            if (with<locality::Construct>::exists(context, id))
                return with<locality::Construct>::get(context, id).body;
            if (with<locality::Scrap>::exists(context, id))
                return with<locality::Scrap>::get(context, id).body;
            if (with<geo::Rock>::exists(context, id))
                return with<geo::Rock>::get(context, id).body;
            if (with<geo::Boulder>::exists(context, id))
                return with<geo::Boulder>::get(context, id).body;
            if (with<locality::Bullet>::exists(context, id))
                return with<locality::Bullet>::get(context, id).body;
            return {};
        }

        auto worldPosOfThing(Reading context, locality::Thing::Id id) -> base::maybe<dvec3> {
            const auto body = bodyOfThing(context, id);
            if (not body or not with<phys::Body>::exists(context, *body))
                return {};
            return with<phys::Body>::get(context, *body).position;
        }

        void applyOrbitPose(Writing context, scene::Camera::Id camera, const controller::CameraOrbit::Quantum& orbit) {
            HPB hpb = orbit.hpb;
            hpb.z = 0.0f;
            const quat rotation = Pose::from(Pos{0.0f, 0.0f, 0.0f}, hpb).rotation;
            const vec3 forward = glm::normalize(rotation * vec3{0.0f, 0.0f, -1.0f});
            auto node = with<scene::Node>::modify(context, camera);
            node->pose.rotation = rotation;
            node->pose.position = orbit.pivot - forward * orbit.distance;
        }

        void aimOrbitAt(Writing context, scene::Camera::Id camera, Pos pivot) {
            auto orbit = with<controller::CameraOrbit>::modify(context, camera);
            const auto& node = with<scene::Node>::get(context, camera);
            HPB hpb = node.pose.hpb();
            hpb.z = 0.0f;
            float distance = orbit->distance;
            const vec3 toCamera = node.pose.position - pivot;
            if (glm::dot(toCamera, toCamera) > 1e-8f) {
                distance = std::clamp(glm::length(toCamera), orbitDistanceMin, orbitDistanceMax);
                const vec3 forward = glm::normalize(-toCamera);
                const float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
                const float heading = std::atan2(forward.x, -forward.z);
                hpb = HPB{glm::degrees(heading), glm::degrees(pitch), 0.0f};
            }
            orbit->pivot = pivot;
            orbit->hpb = hpb;
            orbit->distance = distance;
            applyOrbitPose(context, camera, *orbit);
        }

        template<typename Aspect>
        void removeEvery(Writing context) {
            vector<typename Aspect::Id> ids;
            for (const auto& entry : context->aspect<Aspect>().items())
                ids.push_back(entry.id);
            for (const auto id : ids) {
                if (with<Aspect>::exists(context, id))
                    with<Aspect>::remove(context, id);
            }
        }

    } // namespace

    Schema Game::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<phys::Body>(),
            ask::schema::aspect<phys::rigid::Crystal>(),
            ask::schema::aspect<phys::rigid::Solid>(),
            ask::schema::aspect<phys::rigid::Ray>(),
            ask::schema::aspect<phys::rigid::CelestialGravity>(),
            ask::schema::aspect<phys::Resting>(),
            ask::schema::aspect<locality::Thing>(),
            ask::schema::aspect<locality::Flash>(),
            ask::schema::aspect<locality::Bullet>(),
            ask::schema::aspect<locality::Construct>(),
            ask::schema::aspect<locality::Scrap>(),
            ask::schema::aspect<decorations::Dust>(),
            ask::schema::aspect<geo::Rock>(),
            ask::schema::aspect<geo::Boulder>(),
            ask::schema::aspect<resource::Assets>(),
            ask::schema::aspect<mech::Blueprint>(),
            ask::schema::aspect<mech::Mount>(),
            ask::schema::aspect<resource::SkySphereGenerator>(),
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
        if (not blueprints.addAssets(context, *shared))
            return;
        if (not starMap.visuals.addAssets(context))
            return;
        strategic.loadResources(context, *shared);
    }

    void Game::prepareAssets(Writing) {
    }

    // Locality. Not entered from the map yet.
    void Game::populateWorld(Writing context, system::Window::Id window) {
        {
            auto world = with<World>::modify_global(context);
            world->window = window;
            world->paused = true;
        }
        with<locality::Thing>::modify_global(context)->timeScale = 1.0f;

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.0f, 0.0f, 0.0f, 1.0f},
        });

        const auto root = with<locality::Thing>::get_global(context).scene;
        with<scene::Root>::modify(context, root)->ambient_intensity = 0.18f;
        physics.emplace(root);

        if (not with<resource::SkySphereGenerator>::materialize(context, *assets.skySphereGeometry, window)) {
            return (void)context.refuse("eltanin::Game::populateWorld: sky geometry materialization failed");
        }
        if (not assets.scrap or not resource::ScrapBox::materialize(context, *assets.scrap, window)) {
            return (void)context.refuse("eltanin::Game::populateWorld: scrap geometry materialization failed");
        }

        const auto gridId = with<scene::Interface>::createGrid(context, root, window, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Grid>{.geometry = *assets.primitive.grid, .material = *shared->material.grid, .opacity = 0.35f, .patternScale = 1.0f});
        if (with<scene::Node>::exists(context, gridId)) {
            scene::Node::Actions::setVisible(context, gridId, false);
            grid = gridId;
        }

        if (not assets.sprites) {
            return (void)context.refuse("eltanin::Game::populateWorld: sprites texpack missing");
        }
        const auto skyResolved = ::rmmr::resource::meshpack::Asset::Resolved{
            .geometry = *assets.skySphereGeometry,
            .entry = ::rmmr::resource::geometry::EntryId{0},
            .surfaces = {{::rmmr::resource::geometry::SurfaceId{0}, ::rmmr::resource::material::Instance{.material = *assets.skySphereMaterial, .textures = {{"albedoMap", "skySphere.png"}}}}},
            .texpack = assets.sprites,
        };
        const auto skyScale = geo::Horizon::stars / geo::Horizon::skyMesh;
        const auto sky = with<scene::Interface>::createMeshActor(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), skyResolved, with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{skyScale, skyScale, skyScale}));
        if (not assets.primitive.sphere or not assets.skyBackdropMaterial)
            return (void)context.refuse("eltanin::Game::populateWorld: sky backdrop missing");
        const auto backdropResolved = ::rmmr::resource::meshpack::Asset::Resolved{
            .geometry = *assets.primitive.sphere,
            .entry = ::rmmr::resource::geometry::EntryId{0},
            .surfaces = {{::rmmr::resource::geometry::SurfaceId{0}, ::rmmr::resource::material::Instance{.material = *assets.skyBackdropMaterial, .textures = {}}}},
            .texpack = {},
        };
        const auto skyBackdrop = with<scene::Interface>::createMeshActor(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), backdropResolved, with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{-geo::Horizon::backdrop, -geo::Horizon::backdrop, -geo::Horizon::backdrop}));

        const auto camera = with<scene::Interface>::createCamera(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), 100.0f * std::numbers::pi_v<float> / 180.0f);
        {
            auto quantum = with<scene::Camera>::modify(context, camera);
            quantum->z_near = geo::Horizon::near;
            quantum->z_far = geo::Horizon::far;
        }
        const auto freeInput = with<system::ViewInput>::create(context);
        with<controller::Camera3d>::create(context, camera, freeInput);

        bindGameEntities(context);
        {
            auto world = with<World>::modify_global(context);
            world->sky = sky;
            world->skyBackdrop = skyBackdrop;
            world->camera = camera;
        }
        strategic.populate(context, window);
        if (physics)
            physics->planet = planet ? &*planet : nullptr;
        if (not geo::Sun::placed())
            geo::Sun::place(context, geo::Sun::sol());
        // TODO: use this for some scenarios as time-saver: ui.assembler.spawnVel = vec3{0.0f, 0.0f, 10.0f}; // temporary: +Z approach toward ice asteroid

        with<World>::tetherEnvironment(context);

        world_view = View{.viewport = viewport, .scene = root, .camera = camera};
        views = {*world_view};

        {
            const auto& freePose = with<scene::Node>::get(context, camera).pose;
            const auto spectator = with<scene::Interface>::createCamera(context, root, freePose, spectatorFov);
            {
                auto quantum = with<scene::Camera>::modify(context, spectator);
                quantum->z_near = geo::Horizon::near;
                quantum->z_far = geo::Horizon::far;
            }
            const auto spectatorInput = with<system::ViewInput>::create(context);
            with<controller::CameraOrbit>::create(context, spectator, spectatorInput, freePose.position, 24.0f);
            cameras.emplace(Cameras{.kind = Cameras::Kind::free, .free = camera, .spectator = spectator, .freeInput = freeInput, .spectatorInput = spectatorInput, .hotkeyDown = false});
        }

        const auto manager = with<::rmmr::resource::Manager>::singleton(context);
        blueprintPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "blueprints");
        mountPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "fittings");
        blueprints.create(context);
    }

    void Game::bindGameEntities(Writing context) {
        with<locality::Bullet>::bindResources(context);
        with<decorations::Dust>::bindResources(context);
        with<locality::Scrap>::bindResources(context);
        with<locality::Flash>::bindResources(context);
        with<locality::Construct>::bindResources(context);
        with<geo::Rock>::bindResources(context);
        with<geo::Boulder>::bindResources(context);
    }

    void Game::setup(Writing context, system::Window::Id window) {
        uiMode = UiMode::starMap;
        {
            auto world = with<World>::modify_global(context);
            world->window = window;
            world->paused = true;
        }
        starMap.open(context, window);
        if (starMap.view)
            views = {*starMap.view};
        const auto manager = with<::rmmr::resource::Manager>::singleton(context);
        blueprintPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "blueprints");
        mountPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "fittings");
        blueprints.create(context);
        engageInputs(context);
    }

    void Game::advanceSim(Writing context, seconds dt) {
        with<locality::Thing>::update(context, dt);
    }

    void Game::onFrame(establish::Realm& world, int64 dt_us) {
        if (not mountPack.ready)
            mountPack.loadFromDisk(world);
        if (not blueprintPack.ready) {
            blueprintPack.loadFromDisk(world);
            if (blueprintPack.unnamed)
                world.branch([&](Writing context) { blueprints.show(context, *blueprintPack.unnamed); });
        }
        const seconds wallDt = static_cast<seconds>(dt_us) / 1'000'000.0;
        const seconds simDt = with<World>::get_global(world).paused ? seconds{0} : wallDt * static_cast<seconds>(with<locality::Thing>::get_global(world).timeScale);
        if (physics)
            physics->step(world, simDt);
        advanceSim(world, simDt);
        handleCameraHotkey(world);
        trackSpectator(world);
        if (uiMode == UiMode::starMap)
            starMap.follow(world);
        with<World>::tetherEnvironment(world);
        if (planet and uiMode == UiMode::locality) {
            if (const auto camera = with<World>::get_global(world).camera; camera and with<scene::Node>::exists(world, *camera))
                planet->update(world, with<scene::Node>::get(world, *camera).pose.position);
        }
    }

    void Game::presentCamera(Writing context, scene::Camera::Id camera) {
        with<World>::modify_global(context)->camera = camera;
        if (not world_view)
            return;
        world_view->camera = camera;
        views = {*world_view};
    }

    auto Game::focusCenter(Reading context) const -> base::maybe<dvec3> {
        dvec3 sum{0.0, 0.0, 0.0};
        integer count = 0;
        for (const auto id : focus.things) {
            const auto pos = worldPosOfThing(context, id);
            if (not pos)
                continue;
            sum += *pos;
            ++count;
        }
        if (count == 0)
            return {};
        return sum / static_cast<double>(count);
    }

    void Game::setCameraKind(Writing context, Cameras::Kind kind) {
        if (not cameras or kind == cameras->kind)
            return;
        if (kind == Cameras::Kind::spectator) {
            const auto center = focusCenter(context);
            if (not center or not with<controller::CameraOrbit>::exists(context, cameras->spectator))
                return;
            with<scene::Node>::modify(context, cameras->spectator)->pose = with<scene::Node>::get(context, cameras->free).pose;
            aimOrbitAt(context, cameras->spectator, vec3{*center});
            cameras->kind = Cameras::Kind::spectator;
            presentCamera(context, cameras->spectator);
            return;
        }
        with<scene::Node>::modify(context, cameras->free)->pose = with<scene::Node>::get(context, cameras->spectator).pose;
        cameras->kind = Cameras::Kind::free;
        presentCamera(context, cameras->free);
    }

    void Game::engageInputs(Writing context) {
        const bool editor = uiMode == UiMode::starMap and starMap.menu.blueprints.has_value();
        const bool map = uiMode == UiMode::starMap and not editor;
        const bool locality = uiMode == UiMode::locality;
        const bool freeCam = locality and cameras and cameras->kind == Cameras::Kind::free;
        const bool spectatorCam = locality and cameras and cameras->kind == Cameras::Kind::spectator;
        if (starMap.input)
            with<system::ViewInput>::engage(context, *starMap.input, map);
        if (blueprints.state.mainScene.input)
            with<system::ViewInput>::engage(context, *blueprints.state.mainScene.input, editor and not blueprints.state.paletteMode);
        if (blueprints.state.paletteScene.input)
            with<system::ViewInput>::engage(context, *blueprints.state.paletteScene.input, editor and blueprints.state.paletteMode);
        if (cameras) {
            with<system::ViewInput>::engage(context, cameras->freeInput, freeCam);
            with<system::ViewInput>::engage(context, cameras->spectatorInput, spectatorCam);
        }
    }

    void Game::handleCameraHotkey(Writing context) {
        if (not cameras or uiMode != UiMode::locality)
            return;
        const auto mail = cameras->kind == Cameras::Kind::free ? cameras->freeInput : cameras->spectatorInput;
        const bool down = keyDown(with<system::ViewInput>::get(context, mail).keys, GLFW_KEY_V);
        if (down and not cameras->hotkeyDown) {
            if (cameras->kind == Cameras::Kind::free)
                setCameraKind(context, Cameras::Kind::spectator);
            else
                setCameraKind(context, Cameras::Kind::free);
        }
        cameras->hotkeyDown = down;
    }

    void Game::trackSpectator(Writing context) {
        if (not cameras or cameras->kind != Cameras::Kind::spectator)
            return;
        const auto center = focusCenter(context);
        if (not center) {
            setCameraKind(context, Cameras::Kind::free);
            return;
        }
        if (not with<controller::CameraOrbit>::exists(context, cameras->spectator))
            return;
        auto orbit = with<controller::CameraOrbit>::modify(context, cameras->spectator);
        orbit->pivot = vec3{*center};
        applyOrbitPose(context, cameras->spectator, *orbit);
    }

    void Game::clearLocalityPopulation(Writing context) {
        removeEvery<locality::Flash>(context);
        removeEvery<locality::Bullet>(context);
        removeEvery<locality::Scrap>(context);
        removeEvery<locality::Construct>(context);
        removeEvery<geo::Rock>(context);
        removeEvery<geo::Boulder>(context);
        focus.things.clear();
    }

    void Game::openPlanetScenario(Writing context) {
        if (uiMode == UiMode::locality)
            return;
        const auto bound = with<World>::get_global(context).window;
        if (not bound)
            return;
        if (not world_view)
            populateWorld(context, *bound);
        if (not world_view)
            return;
        planeliod.placePlanet(context, *bound, planet);
        planeliod.populate(context, *bound);
        if (physics)
            physics->planet = planet ? &*planet : nullptr;
        uiMode = UiMode::locality;
        views = {*world_view};
        starMap.menu.blueprints.reset();
    }

    void Game::closeLocalityScenario(Writing context) {
        if (uiMode != UiMode::locality)
            return;
        clearLocalityPopulation(context);
        if (planet) {
            planet->dismantle(context);
            planet.reset();
        }
        if (physics)
            physics->planet = nullptr;
        ui.blueprints.reset();
        ui.physics.reset();
        uiMode = UiMode::starMap;
        if (starMap.view)
            views = {*starMap.view};
    }

}
