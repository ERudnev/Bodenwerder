#include "story.h"

#include "mech/semantics/together.include.h"

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
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/viewport.q1.h>

#include <format>
#include <numbers>
#include <string>
#include <string_view>

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
            ask::schema::aspect<resource::blueprint::Asset>(),
            ask::schema::aspect<resource::blueprint::Loader>(),
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

        assets.skySphere = with<Assets>::add_sprites_kenney(
            context,
            Name::from("Eltanin", "skySphere"),
            item<sprite::LoaderKenney>{
                .image = "sprites/skySphere.png",
                .descriptor = "sprites/skySphere.xml",
            });

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

        const auto& sky_sphere_pack = with<sprite::Pack>::get(context, *assets.skySphere);
        assets.skySphereMaterial = with<Assets>::add_material(
            context,
            Name::from("Eltanin", "skySphere"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::environment, Material::Technique{
                        .program = with<Unit>::remember(context, sky_sphere_shader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({
                            "model",
                            "view",
                            "projection",
                            "albedo",
                            "albedoMap",
                        }),
                        .textures = {
                            Material::TextureBinding{
                                .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                                .texture = sky_sphere_pack.texture,
                            },
                        },
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });

        // LevelOne surfaces: mount/outer textured; type = engine lit_transparent (no map).
        if (not shared or shared->material.debugLitTextured.empty()) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit_textured etalon missing");
        }
        if (not shared->material.litTransparent) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit_transparent missing");
        }
        const auto etalon = shared->material.debugLitTextured[0];
        const auto mech = [&](const char* own, filename file) {
            (void)with<Assets>::compose_material(context, Name::from("Eltanin", own), std::move(file), etalon);
        };
        mech("mtile05", "textures/mech/mtile05.jpg");
        mech("pewter2", "textures/mech/pewter2.bmp");
        mech("panelTech", "textures/mech/panel_tech_1.bmp");
        mech("kosmosWall", "textures/mech/CH_T_KOSMOSSCIANAA.JPG");
        mech("metal10469", "textures/mech/10469.jpg");
        mech("metal10469v3", "textures/mech/10469-v3.jpg");
        mech("mount", "textures/mech/pewter2.bmp");
        mech("outer", "textures/mech/mtile05.jpg");
        (void)with<Assets>::add_material(context, Name::from("Eltanin", "type"), with<Material>::get(context, *shared->material.litTransparent));

        assets.levelOne = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "levelOne"),
            item<meshpack::LoaderLwo>{.file = "meshes/system/levelOne/levelOne.lwo.meshpack"});
        assets.levelTwo = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "levelTwo"),
            item<meshpack::LoaderLwo>{.file = "meshes/system/levelTwo/levelTwo.lwo.meshpack"});
        assets.interframe = with<Assets>::add_meshpack_lwo_loader(
            context,
            Name::from("Eltanin", "interframe"),
            item<meshpack::LoaderLwo>{.file = "meshes/system/levelOne/interframe.lwo.meshpack"});

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

        with<resource::SkySphereGenerator>::materialize(context, *assets.skySphereGeometry, window);

        with<scene::Interface>::createGrid(context, root,
            Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Grid>{.geometry = *assets.primitive.grid, .material = *shared->material.grid, .opacity = 0.35f, .pattern_scale = 1.0f});

        const auto sky = with<scene::Interface>::createSimpleActor(context, root,
            Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::actor::Simple>{
                .geometry = *assets.skySphereGeometry,
                .material = *assets.skySphereMaterial,
                .albedo = RGB{1.0f, 1.0f, 1.0f},
            });

        const Pos cameraPos{8.0f, 6.0f, 16.0f};
        const Pos cameraTarget{0.0f, 0.0f, -2.0f};
        const Pose cameraPose{.position = cameraPos, .rotation = glm::quatLookAt(glm::normalize(cameraTarget - cameraPos), vec3{0.0f, 1.0f, 0.0f})};
        const auto camera = with<scene::Interface>::createCamera(context, root, cameraPose, 100.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root,
            Pose::from(Pos{9.5f, 19.0f, 7.5f}, HPB{0.0f, 0.0f, 0.0f}),
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        if (not assets.interframe) {
            return (void)context.refuse("eltanin::Game::populateWorld: interframe meshpack missing");
        }
        {
            using namespace mech::subframe;
            const auto pack = *assets.interframe;

            const auto scenePose = [](mech::orient::key ori) -> Pose {
                return Pose{
                    .position = Pos{0.0f, 0.0f, 0.0f},
                    .rotation = glm::normalize(glm::quat_cast(glm::mat3(mech::orient::matrix[static_cast<std::size_t>(ori)]))),
                };
            };

            const auto spawnEntry = [&](std::string_view entry, mech::orient::key ori) {
                const auto resolved = ::rmmr::resource::meshpack::Asset::Actions::resolve(context, pack, std::string{entry});
                if (not resolved) {
                    return (void)context.refuse(std::format("eltanin::Game::populateWorld: interframe entry '{}' missing", entry));
                }
                with<scene::Interface>::createMeshActor(
                    context,
                    root,
                    scenePose(ori),
                    item<scene::actor::Mesh>{
                        .geometry = resolved->geometry,
                        .materials = resolved->materials,
                        .albedo = RGB{1.0f, 1.0f, 1.0f},
                        .scale = vec3{1.0f, 1.0f, 1.0f},
                        .opacity = 1.0f,
                        .visible = true,
                    });
            };

            const auto cornerEntry = [](corner::kind kind) -> std::string_view {
                switch (kind) {
                    case corner::kind::c124: return "c124";
                    case corner::kind::c1364: return "c1364";
                    case corner::kind::c164: return "c164";
                    case corner::kind::c134: return "c134";
                    case corner::kind::c135: return "c135";
                    case corner::kind::c12: return "c12";
                    case corner::kind::c13: return "c13";
                    case corner::kind::c15: return "c15";
                    case corner::kind::c16: return "c16";
                    case corner::kind::c34: return "c34";
                    case corner::kind::c35: return "c35";
                }
                return {};
            };

            // LWO layer typo: he1ged90s (not he1deg90s).
            const auto halfEdgeEntry = [](halfEdge::kind kind, halfEdge::Pole pole) -> std::string {
                const auto& spec = halfEdge::specs.at(kind);
                const char poleTag = pole == halfEdge::Pole::s ? 's' : 'e';
                if (kind == halfEdge::kind::he1deg90 and pole == halfEdge::Pole::s)
                    return std::format("he1ged90{}", poleTag);
                return std::format("{}{}", spec.code, poleTag);
            };

            const auto& recipe = recipes.at(mech::frame::shape::k7);
            for (const auto& piece : recipe.corners)
                spawnEntry(cornerEntry(piece.kind), piece.orient);

            for (const auto& edge : recipe.edges) {
                const auto poleAtMesh0 = edge.poleAtMesh0;
                const auto poleAtMeshRay = halfEdge::opposite(edge.poleAtMesh0);
                spawnEntry(halfEdgeEntry(edge.kind, poleAtMesh0), edge.orient);
                spawnEntry(halfEdgeEntry(edge.kind, poleAtMeshRay), edge.orient);
            }
        }

        physics_ui.shapeMaterial = shared->material.gizmo.textured;
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
        blueprints.create(context, with<::rmmr::resource::Manager>::get(context, *manager).location / "Eltanin" / "blueprints");
    }

    void Game::setup(Writing context, system::Window::Id window) {
        populateWorld(context, window);
    }

    void Game::advanceSim(Writing context, int64 dt_us) {
        with<World>::advance(context, dt_us);
    }

    void Game::onFrame(establish::Realm& world, int64 dt_us) {
        with<World>::tetherEnvironment(world);
        physics.step(world, dt_us);
        advanceSim(world, dt_us);
    }

}
