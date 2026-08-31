#include "story.h"

#include <eltanin/locality/thing.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/scrap.q1.h>
#include <eltanin/decorations/dust.q1.h>
#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/compound.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/mount.q1.h>
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
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/viewport.q1.h>

#include <numbers>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema Game::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<phys::Body>(),
            ask::schema::aspect<phys::Compound>(),
            ask::schema::aspect<phys::rigid::Crystal>(),
            ask::schema::aspect<phys::rigid::Solid>(),
            ask::schema::aspect<phys::rigid::Ray>(),
            ask::schema::aspect<phys::rigid::CelestialGravity>(),
            ask::schema::aspect<locality::Thing>(),
            ask::schema::aspect<locality::Flash>(),
            ask::schema::aspect<locality::Bullet>(),
            ask::schema::aspect<locality::Construct>(),
            ask::schema::aspect<locality::Scrap>(),
            ask::schema::aspect<decorations::Dust>(),
            ask::schema::aspect<locality::geo::Rock>(),
            ask::schema::aspect<locality::geo::Boulder>(),
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
        using namespace ::rmmr::resource;
        using geometry::Generator;
        using Assets = ::rmmr::resource::Assets;
        using Name = Unit::Name;
        using Material = ::rmmr::resource::material::Asset;

        assets.primitive.grid = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "grid"), item<Generator>{.type = Generator::Type::gridPlane, .subdivisions = 0});
        assets.primitive.sphere = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "sphere"), item<Generator>{.type = Generator::Type::sphere, .subdivisions = 1});
        assets.primitive.kube = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "kube"), item<Generator>{.type = Generator::Type::kube, .subdivisions = 0});
        assets.primitive.diamond = with<Assets>::add_geometry_generator(context, Name::from("rmmr", "diamond"), item<Generator>{.type = Generator::Type::diamond, .subdivisions = 0});

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
                        .glowSpread = false,
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });

        const auto flashShader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "flash"), item<shader::Loader>{.vertex = "shaders/flash.vert.glsl", .fragment = "shaders/flash.frag.glsl"});
        const auto flashGlowShader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "flashGlow"), item<shader::Loader>{.vertex = "shaders/flash.vert.glsl", .fragment = "shaders/flashGlow.frag.glsl"});
        const auto dustShader = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "dust"), item<shader::Loader>{.vertex = "shaders/flash.vert.glsl", .fragment = "shaders/dust.frag.glsl"});
        with<Assets>::add_geometry_generator(context, Name::from("Eltanin", "flashSphere"), item<Generator>{.type = Generator::Type::sphere, .subdivisions = 4});
        with<Assets>::add_material(
            context,
            Name::from("Eltanin", "flash"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::transparent, Material::Technique{
                        .program = with<Unit>::remember(context, flashShader),
                        .uniforms = {},
                        .glowSpread = true,
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });
        with<Assets>::add_material(
            context,
            Name::from("Eltanin", "flashGlow"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::transparent, Material::Technique{
                        .program = with<Unit>::remember(context, flashGlowShader),
                        .uniforms = {},
                        .glowSpread = true,
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });
        with<Assets>::add_material(
            context,
            Name::from("Eltanin", "dust"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::transparent, Material::Technique{
                        .program = with<Unit>::remember(context, dustShader),
                        .uniforms = {},
                        .glowSpread = true,
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });

        if (not shared->material.lit) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit missing");
        }
        if (not shared->material.gizmo.textured) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr gizmo_textured missing");
        }
        {
            const auto& gizmoTextured = with<Material>::get(context, *shared->material.gizmo.textured);
            const auto gizmoTechnique = gizmoTextured.techniques.find(renderer::Pass::gizmo);
            if (gizmoTechnique == gizmoTextured.techniques.end())
                return (void)context.refuse("eltanin::Game::addAssets: gizmo_textured technique missing");
            assets.collisionDebugMaterial = with<Assets>::add_material(
                context,
                Name::from("Eltanin", "collisionDebug"),
                Material::Quantum{
                    .techniques = {
                        {renderer::Pass::opaque, Material::Technique{
                            .program = gizmoTechnique->second.program,
                            .uniforms = gizmoTechnique->second.uniforms,
                            .glowSpread = false,
                        }},
                    },
                    .nearest = false,
                    .blend = renderer::BlendMode::inherit,
                });
        }
        scenario.loadResources(context, *shared);

        // Mech albedo catalog; editor meshpacks under meshes/editor.
        if (not shared or not shared->material.litTextured) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit_textured missing");
        }
        if (not shared->material.litTransparent) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit_transparent missing");
        }
        if (not shared->material.lit) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit missing");
        }
        assets.mech = with<Assets>::add_texpack_catalog(
            context,
            Name::from("Eltanin", "mech"),
            item<texpack::LoaderCatalog>{.directory = "textures/mech"},
            index2{1024, 1024},
            32);
        {
            const auto hullProgram = with<Assets>::add_shader_loader(context, Name::from("Eltanin", "hull"), item<shader::Loader>{.vertex = "shaders/hull.vert.glsl", .fragment = "shaders/hull.frag.glsl"});
            auto hull = with<Material>::get(context, *shared->material.litTextured);
            const auto opaque = hull.techniques.find(renderer::Pass::opaque);
            if (opaque == hull.techniques.end()) {
                return (void)context.refuse("eltanin::Game::addAssets: lit_textured has no opaque pass");
            }
            opaque->second.program = with<Unit>::remember(context, hullProgram);
            opaque->second.glowSpread = true;
            (void)with<Assets>::add_material(context, Name::from("Eltanin", "hull"), std::move(hull));
        }
        // Transparent: world cursor. Opaque: role-colored placeholder boxes (attachments).
        (void)with<Assets>::add_material(context, Name::from("Eltanin", "type"), with<Material>::get(context, *shared->material.litTransparent));
        (void)with<Assets>::add_material(context, Name::from("Eltanin", "typeSolid"), with<Material>::get(context, *shared->material.lit));
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
        assets.armour = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "armour"),
            item<meshpack::LoaderLwo>{.file = "meshes/fittings/mounts/armour.lwo.meshpack", .geometry = {}, .pending = {}});
        assets.devices = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "devices"),
            item<meshpack::LoaderLwo>{.file = "meshes/fittings/devices/cannon_temp_solid.lwo.meshpack", .geometry = {}, .pending = {}});
        assets.projectiles = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "projectiles"),
            item<meshpack::LoaderLwo>{.file = "meshes/misc/projectiles.lwo.meshpack", .geometry = {}, .pending = {}});

        const auto manager = with<Manager>::singleton(context);
        const auto sky_geometry_id = with<Unit_group>::addElement(context, manager, Unit::Quantum{.name = Name::from("Eltanin", "skySphere")});
        with<geometry::Asset>::extend(context, sky_geometry_id, geometry::Asset::Quantum{});
        with<resource::SkySphereGenerator>::extend(context, sky_geometry_id, resource::SkySphereGenerator::Quantum{
            .count = 48800, // 20k×2, then halo ×2 again (~31k disk + ~18k halo)
            .seed = 1,
            .angular_diameter_deg = 0.41f,
        });
        assets.skySphereGeometry = sky_geometry_id;

        const auto scrap_id = with<Unit_group>::addElement(context, manager, Unit::Quantum{.name = Name::from("Eltanin", "scrap")});
        with<geometry::Asset>::extend(context, scrap_id, geometry::Asset::Quantum{.entries = {}, .surfaces = {}, .mounts = {}, .entryCatalog = {}, .surfaceCatalogs = {}});
        assets.scrap = scrap_id;
    }

    void Game::prepareAssets(Writing) {
    }

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
        physics.emplace(root);

        if (not with<resource::SkySphereGenerator>::materialize(context, *assets.skySphereGeometry, window)) {
            return (void)context.refuse("eltanin::Game::populateWorld: sky geometry materialization failed");
        }
        if (not assets.scrap or not resource::ScrapBox::materialize(context, *assets.scrap, window)) {
            return (void)context.refuse("eltanin::Game::populateWorld: scrap geometry materialization failed");
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

        const Pos cameraPos{0.0f, 55.0f, 200.0f};
        const Pos cameraTarget{0.0f, 0.0f, 0.0f};
        const Pose cameraPose{.position = cameraPos, .rotation = glm::quatLookAt(glm::normalize(cameraTarget - cameraPos), vec3{0.0f, 1.0f, 0.0f})};
        const auto camera = with<scene::Interface>::createCamera(context, root, cameraPose, 100.0f * std::numbers::pi_v<float> / 180.0f);
        {
            // Local frame ~8192 m; 24-bit depth, no reverse-Z → near stays ≥1 m (far/near ≈ 16k).
            auto quantum = with<scene::Camera>::modify(context, camera);
            quantum->z_near = 1.0f;
            quantum->z_far = 16384.0f;
        }
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{-25.0f, -30.0f, 0.0f}), item<scene::Light>{.kind = scene::Light::Kind::directional, .color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 8.0f, .range = 0.0f});

        bindGameEntities(context);
        scenario.populate(context, window);

        {
            auto world = with<World>::modify_global(context);
            world->sky = sky;
            world->camera = camera;
        }
        with<World>::tetherEnvironment(context);

        world_view = View{.viewport = viewport, .scene = root, .camera = camera};
        views = {*world_view};

        const auto manager = with<::rmmr::resource::Manager>::singleton(context);
        blueprintPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "blueprints");
        mountPack.bind(with<::rmmr::resource::Manager>::get(context, manager).location / "Eltanin" / "fittings" / "mounts");
        blueprints.create(context);
    }

    void Game::bindGameEntities(Writing context) {
        with<locality::Bullet>::bindResources(context);
        with<decorations::Dust>::bindResources(context);
        with<locality::Scrap>::bindResources(context);
        with<locality::Flash>::bindResources(context);
        with<locality::Construct>::bindResources(context);
        with<locality::geo::Rock>::bindResources(context);
        with<locality::geo::Boulder>::bindResources(context);
    }

    void Game::setup(Writing context, system::Window::Id window) {
        populateWorld(context, window);
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
        with<World>::tetherEnvironment(world);
        const seconds wallDt = static_cast<seconds>(dt_us) / 1'000'000.0;
        const seconds simDt = with<World>::get_global(world).paused ? seconds{0} : wallDt * static_cast<seconds>(with<locality::Thing>::get_global(world).timeScale);
        if (physics)
            physics->step(world, simDt);
        advanceSim(world, simDt);
    }

}
