#include "views/locality/view.h"

#include "geo/celestial/horizon.h"
#include "geo/celestial/sun.h"
#include "locality/assets.h"

#include <eltanin/decorations/dust.q1.h>
#include <eltanin/geo/boulder.q1.h>
#include <eltanin/geo/rock.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/resources/geometry.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/controller/cameraOrbit.q1.h>
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

namespace eltanin::views::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float spectatorFov = 60.0f * std::numbers::pi_v<float> / 180.0f;
        constexpr float orbitDistanceMin = 0.5f;
        constexpr float orbitDistanceMax = 500.0f;

        auto keyDown(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() and keys[static_cast<std::size_t>(key)];
        }

        auto bodyOfThing(Reading context, ::eltanin::locality::Thing::Id id) -> base::maybe<phys::Body::Id> {
            if (with<::eltanin::locality::Construct>::exists(context, id))
                return with<::eltanin::locality::Construct>::get(context, id).body;
            if (with<::eltanin::locality::Scrap>::exists(context, id))
                return with<::eltanin::locality::Scrap>::get(context, id).body;
            if (with<geo::Rock>::exists(context, id))
                return with<geo::Rock>::get(context, id).body;
            if (with<geo::Boulder>::exists(context, id))
                return with<geo::Boulder>::get(context, id).body;
            if (with<::eltanin::locality::Bullet>::exists(context, id))
                return with<::eltanin::locality::Bullet>::get(context, id).body;
            return {};
        }

        auto worldPosOfThing(Reading context, ::eltanin::locality::Thing::Id id) -> base::maybe<dvec3> {
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

    } // namespace

    auto View::create(Writing context, system::Window::Id window, const assets::Handles& assets) -> bool {
        panels.assembler = Panels::Assembler{
            .blueprint = {},
            .spawnPos = Pos{0.0f, 0.0f, 0.0f},
            .spawnHpb = HPB{0.0f, 0.0f, 0.0f},
            .spawnVel = vec3{0.0f, 0.0f, 0.0f},
            .spawnAtCamera = false,
        };

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.0f, 0.0f, 0.0f, 1.0f},
        });

        const auto root = with<::eltanin::locality::Thing>::get_global(context).scene;
        with<scene::Root>::modify(context, root)->ambient_intensity = 0.18f;
        physics.emplace(root);

        if (not with<::eltanin::resource::SkySphereGenerator>::materialize(context, *assets.skySphereGeometry, window)) {
            context.refuse("eltanin::views::locality::View::create: sky geometry materialization failed");
            return false;
        }
        if (not assets.scrap or not ::eltanin::resource::ScrapBox::materialize(context, *assets.scrap, window)) {
            context.refuse("eltanin::views::locality::View::create: scrap geometry materialization failed");
            return false;
        }
        if (not assets.sprites) {
            context.refuse("eltanin::views::locality::View::create: sprites texpack missing");
            return false;
        }
        const auto skyResolved = ::rmmr::resource::meshpack::Asset::Resolved{
            .geometry = *assets.skySphereGeometry,
            .entry = ::rmmr::resource::geometry::EntryId{0},
            .surfaces = {{::rmmr::resource::geometry::SurfaceId{0}, ::rmmr::resource::material::Instance{.material = *assets.skySphereMaterial, .textures = {{"albedoMap", "skySphere.png"}}}}},
            .texpack = assets.sprites,
        };
        const auto skyScale = geo::Horizon::stars / geo::Horizon::skyMesh;
        const auto sky = with<scene::Interface>::createMeshActor(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), skyResolved, with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, vec3{skyScale, skyScale, skyScale}));
        if (not assets.primitive.sphere or not assets.skyBackdropMaterial) {
            context.refuse("eltanin::views::locality::View::create: sky backdrop missing");
            return false;
        }
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

        bindEntities(context);
        {
            auto world = with<World>::modify_global(context);
            world->sky = sky;
            world->skyBackdrop = skyBackdrop;
            world->camera = camera;
        }
        if (physics)
            physics->planet = planet ? &*planet : nullptr;
        if (not geo::Sun::placed())
            geo::Sun::place(context, geo::Sun::sol());
        with<World>::tetherEnvironment(context);

        view = rmmr::wrapper::Product::View{.viewport = viewport, .scene = root, .camera = camera};

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

        planeliod.placePlanet(context, window, planet);
        planeliod.populate(context, window);
        if (physics)
            physics->planet = planet ? &*planet : nullptr;
        return true;
    }

    void View::bindEntities(Writing context) {
        with<::eltanin::locality::Bullet>::bindResources(context);
        with<decorations::Dust>::bindResources(context);
        with<::eltanin::locality::Scrap>::bindResources(context);
        with<::eltanin::locality::Flash>::bindResources(context);
        with<::eltanin::locality::Construct>::bindResources(context);
        with<geo::Rock>::bindResources(context);
        with<geo::Boulder>::bindResources(context);
    }

    void View::tick(Stewarding context, seconds dt) {
        if (physics)
            physics->step(context, dt);
        with<::eltanin::locality::Thing>::update(context, dt);
        trackSpectator(context);
        with<World>::tetherEnvironment(context);
        if (planet) {
            if (const auto camera = with<World>::get_global(context).camera; camera and with<scene::Node>::exists(context, *camera))
                planet->update(context, with<scene::Node>::get(context, *camera).pose.position);
        }
    }

    void View::presentCamera(Writing context, scene::Camera::Id camera) {
        with<World>::modify_global(context)->camera = camera;
        if (view)
            view->camera = camera;
    }

    void View::present(std::vector<rmmr::wrapper::Product::View>& productViews) const {
        if (view)
            productViews = {*view};
    }

    auto View::focusCenter(Reading context) const -> base::maybe<dvec3> {
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

    void View::setCameraKind(Writing context, Cameras::Kind kind) {
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

    void View::engageCameras(Writing context, bool shown) {
        if (not cameras)
            return;
        const bool freeCam = shown and cameras->kind == Cameras::Kind::free;
        const bool spectatorCam = shown and cameras->kind == Cameras::Kind::spectator;
        with<system::ViewInput>::engage(context, cameras->freeInput, freeCam);
        with<system::ViewInput>::engage(context, cameras->spectatorInput, spectatorCam);
    }

    void View::handleCameraHotkey(Writing context) {
        if (not cameras)
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

    void View::trackSpectator(Writing context) {
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

}
