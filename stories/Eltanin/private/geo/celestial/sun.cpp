#include "geo/celestial/sun.h"

#include <eltanin/locality/thing.q1.h>
#include <rmmr/api/_interface.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <base/maybe.h>

namespace eltanin::locality::geo {

    using namespace fqsm::api;
    using rmmr::HPB;
    using rmmr::Pose;
    using rmmr::Pos;
    using rmmr::RGB;
    using rmmr::vec3;

    namespace {

        base::maybe<rmmr::resource::geometry::Asset::Id> sphere;
        base::maybe<rmmr::resource::material::Asset::Id> material;
        base::maybe<rmmr::scene::actor::Mesh::Id> disk;
        base::maybe<rmmr::scene::Light::Id> light;

        void bind(Writing context) {
            if (sphere and material)
                return;
            sphere = with<rmmr::resource::Assets>::find<rmmr::resource::geometry::Asset>(context, rmmr::resource::Unit::Name::from("rmmr", "sphere"));
            if (not sphere)
                return (void)context.refuse("eltanin::locality::geo::Sun: sphere geometry missing");
            material = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "skySun"));
            if (not material)
                return (void)context.refuse("eltanin::locality::geo::Sun: skySun material missing");
        }

    }

    auto Sun::sol() -> Look {
        return Look{.heading = HPB{-25.0f, -30.0f, 0.0f}, .color = RGB{1.0f, 0.94f, 0.86f}, .brightness = 8.0f, .angularDiameterDeg = 0.53f};
    }

    auto Sun::redGiant() -> Look {
        return Look{.heading = HPB{-18.0f, -12.0f, 0.0f}, .color = RGB{1.0f, 0.38f, 0.10f}, .brightness = 5.5f, .angularDiameterDeg = 5.2f};
    }

    auto Sun::placed() -> bool {
        return disk.has_value();
    }

    void Sun::place(Writing context, Look look) {
        bind(context);
        if (not sphere or not material)
            return (void)context.refuse("eltanin::locality::geo::Sun::place: assets missing");
        const auto scene = with<Thing>::get_global(context).scene;
        if (not light) {
            light = with<rmmr::scene::Interface>::createLight(context, scene, Pose::from(Pos{0.0f, 0.0f, 0.0f}, look.heading), item<rmmr::scene::Light>{.kind = rmmr::scene::Light::Kind::directional, .color = look.color, .intensity = look.brightness, .range = 0.0f});
        } else {
            with<rmmr::scene::Node>::modify(context, *light)->pose = Pose::from(Pos{0.0f, 0.0f, 0.0f}, look.heading);
            auto quantum = with<rmmr::scene::Light>::modify(context, *light);
            quantum->kind = rmmr::scene::Light::Kind::directional;
            quantum->color = look.color;
            quantum->intensity = look.brightness;
            quantum->range = 0.0f;
        }
        with<rmmr::scene::Root>::modify(context, scene)->primaryLight = *light;
        if (not disk) {
            auto state = with<rmmr::scene::actor::MeshState>::defaults(look.color, 1.0f, vec3{-100.0f, -100.0f, -100.0f});
            state.patternScale = look.angularDiameterDeg;
            const auto resolved = rmmr::resource::meshpack::Asset::Resolved{.geometry = *sphere, .entry = rmmr::resource::geometry::EntryId{0}, .surfaces = {{rmmr::resource::geometry::SurfaceId{0}, rmmr::resource::material::Instance{.material = *material, .textures = {}}}}, .texpack = {}};
            disk = with<rmmr::scene::Interface>::createMeshActor(context, scene, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), resolved, state);
        }
        if (not disk or not with<rmmr::scene::actor::MeshState>::exists(context, *disk))
            return (void)context.refuse("eltanin::locality::geo::Sun::place: disk missing");
        auto meshState = with<rmmr::scene::actor::MeshState>::modify(context, *disk);
        meshState->albedo = look.color;
        meshState->scale = vec3{-100.0f, -100.0f, -100.0f};
        meshState->patternScale = look.angularDiameterDeg;
        meshState->opacity = 1.0f;
    }

    void Sun::tether(Writing context, rmmr::Pos camera) {
        if (not disk or not with<rmmr::scene::Node>::exists(context, *disk))
            return;
        with<rmmr::scene::Node>::modify(context, *disk)->pose.position = camera;
    }

}
