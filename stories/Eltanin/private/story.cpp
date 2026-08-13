#include "story.h"

#include <eltanin/entities/block.q1.h>
#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <eltanin/physics/strong.q1.h>
#include <eltanin/resources/assets.q1.h>
#include <eltanin/resources/geometry.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/viewport.q1.h>

#include <numbers>
#include <utility>

#include <glm/gtc/quaternion.hpp>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema Game::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<phys::Particle>(),
            ask::schema::aspect<phys::Atomic>(),
            ask::schema::aspect<phys::strong::Nail>(),
            ask::schema::aspect<phys::strong::Gluon>(),
            ask::schema::aspect<Block>(),
            ask::schema::aspect<resource::Assets>(),
            ask::schema::aspect<mech::Blueprint>(),
            ask::schema::aspect<resource::SkySphereGenerator>(),
        });
    }

    void Game::createCore(Writing context) {
        const auto host = with<::rmmr::resource::Assets>::singleton(context);
        if (not host) return (void)context.refuse("eltanin::Game::createCore: rmmr Assets singleton missing");
        with<::eltanin::resource::Assets>::extend(context, *host, ::eltanin::resource::Assets::Quantum{});
    }

    void Game::addAssets(Writing context) {
        using namespace ::rmmr::resource;
        using geometry::Generator;
        using Assets = ::rmmr::resource::Assets;
        using Name = Unit::Name;
        using Material = ::rmmr::resource::material::Asset;

        assets.primitive.grid = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "grid"), item<Generator>{.type = Generator::Type::gridPlane});
        assets.primitive.sphere = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "sphere"), item<Generator>{.type = Generator::Type::sphere});
        assets.primitive.kube = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "kube"), item<Generator>{.type = Generator::Type::kube});
        assets.primitive.diamond = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "diamond"), item<Generator>{.type = Generator::Type::diamond});

        // Pack own name = directory basename; layers = image filenames (skySphere.png).
        assets.sprites = with<Assets>::add_texpack_catalog(
            context,
            Name::from("Eltanin", "sprites"),
            item<texpack::LoaderCatalog>{.directory = "sprites"},
            index2{1024, 1024},
            8);

        const auto sky_sphere_shader = with<Assets>::add_shader_loader(
            context,
            Name::from("Eltanin", "skySphere"),
            item<shader::Loader>{
                .vertex = "shaders/skySphere.vert.glsl",
                .fragment = "shaders/skySphere.frag.glsl",
            });

        const auto blueprints_editor_effect = with<Assets>::add_shader_loader(
            context,
            Name::from("Eltanin", "blueprintsEditorEffect"),
            item<shader::Loader>{
                .vertex = "shaders/blueprintsEditorEffect.vert.glsl",
                .fragment = "shaders/blueprintsEditorEffect.frag.glsl",
            });
        assets.blueprintsEditorEffect = with<Assets>::add_overlay(
            context,
            Name::from("Eltanin", "blueprintsEditorEffect"),
            overlay::Asset::Quantum{
                .program = with<Unit>::remember(context, blueprints_editor_effect),
                .uniforms = ::rmmr::material::Semantics::ids_of({"identiffyMap", "selectedMap", "under"}),
                .scale = overlay::Scale::full,
            });

        assets.skySphereMaterial = with<Assets>::add_material(
            context,
            Name::from("Eltanin", "skySphere"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::environment, Material::Technique{
                        .program = with<Unit>::remember(context, sky_sphere_shader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({"albedoMap"}),
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });

        // Mech albedo catalog; editor meshpacks under meshes/editor.
        if (not shared or not shared->material.litTextured) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit_textured missing");
        }
        if (not shared->material.litTransparent) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit_transparent missing");
        }
        assets.mech = with<Assets>::add_texpack_catalog(
            context,
            Name::from("Eltanin", "mech"),
            item<texpack::LoaderCatalog>{.directory = "textures/mech"},
            index2{1024, 1024},
            32);
        (void)with<Assets>::add_material(context, Name::from("Eltanin", "type"), with<Material>::get(context, *shared->material.litTransparent));
        {
            auto ghost = with<Material>::get(context, *shared->material.litTransparent);
            ghost.blend = renderer::BlendMode::additive;
            (void)with<Assets>::add_material(context, Name::from("Eltanin", "clipboardGhost"), std::move(ghost));
        }

        assets.interframe = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "interframe"),
            item<meshpack::LoaderLwo>{.file = "meshes/editor/interframe.lwo.meshpack", .geometry = {}, .pending = {}});
        assets.attachments = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "attachments"),
            item<meshpack::LoaderLwo>{.file = "meshes/editor/attachments.lwo.meshpack", .geometry = {}, .pending = {}});

        const auto manager = *with<Manager>::singleton(context);
        if (not with<Unit_group>::exists(context, manager)) {
            with<Unit_group>::extend(context, manager);
        }
        const auto sky_geometry_id = with<Unit_group>::addElement(context, manager, Unit::Quantum{.name = Name::from("Eltanin", "skySphere")});
        with<geometry::Asset>::extend(context, sky_geometry_id, geometry::Asset::Quantum{});
        with<resource::SkySphereGenerator>::extend(context, sky_geometry_id, resource::SkySphereGenerator::Quantum{
            .count = 48800, // 20k×2, then halo ×2 again (~31k disk + ~18k halo)
            .seed = 1,
            .angular_diameter_deg = 0.41f,
        });
        assets.skySphereGeometry = sky_geometry_id;
    }

    void Game::prepareAssets(Writing) {
    }

    void Game::populateWorld(Writing context, system::Window::Id window) {
        with<World>::modify_global(context)->window = window;

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.0f, 0.0f, 0.0f, 1.0f},
        });

        const auto root = with<scene::Interface>::createScene(context);

        if (not with<resource::SkySphereGenerator>::materialize(context, *assets.skySphereGeometry, window)) {
            return (void)context.refuse("eltanin::Game::populateWorld: sky geometry materialization failed");
        }

        with<scene::Interface>::createGrid(context, root, window,
            Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Grid>{.geometry = *assets.primitive.grid, .material = *shared->material.grid, .opacity = 0.35f, .patternScale = 1.0f});

        if (not assets.sprites) {
            return (void)context.refuse("eltanin::Game::populateWorld: sprites texpack missing");
        }
        const auto skyResolved = ::rmmr::resource::meshpack::Asset::Resolved{
            .geometry = *assets.skySphereGeometry,
            .entry = ::rmmr::resource::geometry::EntryId{0},
            .surfaces = {{::rmmr::resource::geometry::SurfaceId{0}, ::rmmr::resource::material::Instance{.material = *assets.skySphereMaterial, .textures = {{"albedoMap", "skySphere.png"}}}}},
            .texpack = assets.sprites,
        };
        const auto sky = with<scene::Interface>::createMeshActor(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), skyResolved);

        const Pos cameraPos{8.0f, 6.0f, 16.0f};
        const Pos cameraTarget{0.0f, 0.0f, -2.0f};
        const Pose cameraPose{.position = cameraPos, .rotation = glm::quatLookAt(glm::normalize(cameraTarget - cameraPos), vec3{0.0f, 1.0f, 0.0f})};
        const auto camera = with<scene::Interface>::createCamera(context, root, cameraPose, 100.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root, Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        physics_ui.shapeMaterial = shared->material.gizmo.textured;
        if (not shared->texture.debug) {
            return (void)context.refuse("eltanin::Game::populateWorld: rmmr debug texpack missing for gizmo");
        }
        physics_ui.shapeTexpack = shared->texture.debug;
        physics_ui.shapeAlbedoLayer = string{"debug02.jpg"};
        physics_ui.particleGeometry = assets.primitive.diamond;
        physics_ui.particleMaterial = shared->material.gizmo.vertexColor;
        physics_ui.shapeGeometry = assets.primitive.kube;

        {
            auto world = with<World>::modify_global(context);
            world->sky = sky;
            world->camera = camera;
        }
        with<World>::tetherEnvironment(context);

        world_view = View{.viewport = viewport, .scene = root, .camera = camera};
        views = {*world_view};

        const auto manager = with<::rmmr::resource::Manager>::singleton(context);
        if (not manager)
            return (void)context.refuse("eltanin::Game::populateWorld: resource Manager missing");
        blueprintPack.bind(with<::rmmr::resource::Manager>::get(context, *manager).location / "Eltanin" / "blueprints");
        blueprints.create(context);
    }

    void Game::setup(Writing context, system::Window::Id window) {
        populateWorld(context, window);
    }

    void Game::advanceSim(Writing context, int64 dt_us) {
        with<World>::advance(context, dt_us);
    }

    void Game::onFrame(establish::Realm& world, int64 dt_us) {
        if (not blueprintPack.ready) {
            blueprintPack.loadFromDisk(world);
            if (blueprintPack.unnamed)
                world.branch([&](Writing context) { blueprints.show(context, *blueprintPack.unnamed); });
        }
        with<World>::tetherEnvironment(world);
        physics.step(world, dt_us);
        advanceSim(world, dt_us);
    }

}
