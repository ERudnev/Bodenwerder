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
#include <rmmr/resources/necessary.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/viewport.q1.h>

#include <numbers>

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
            ask::schema::aspect<resources::SkySphereGenerator>(),
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

        assets.primitive.grid = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            item<Unit>{.name = "grid", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::gridPlane});

        assets.primitive.sphere = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            item<Unit>{.name = "sphere", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::sphere});

        assets.primitive.kube = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            item<Unit>{.name = "kube", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::kube});

        assets.primitive.diamond = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            item<Unit>{.name = "diamond", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::diamond});

        assets.skySphere = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            item<Unit>{.name = "skySphere", .library = "Eltanin"},
            item<sprite::LoaderKenney>{
                .image = "sprites/skySphere.png",
                .descriptor = "sprites/skySphere.xml",
            });

        const auto sky_sphere_shader = with<::rmmr::resource::Assets>::add_shader_loader(
            context,
            item<Unit>{.name = "skySphere_shader", .library = "Eltanin"},
            item<shader::Loader>{
                .vertex = "shaders/skySphere.vert.glsl",
                .fragment = "shaders/skySphere.frag.glsl",
            });

        const auto& sky_sphere_pack = with<sprite::Pack>::get(context, *assets.skySphere);
        assets.skySphereMaterial = with<::rmmr::resource::Assets>::add_material(
            context,
            item<Unit>{.name = "skySphere_material", .library = "Eltanin"},
            ::rmmr::resource::material::Asset::Quantum{
                .techniques = {
                    {renderer::Pass::environment, ::rmmr::resource::material::Asset::Technique{
                        .program = with<Unit>::remember(context, sky_sphere_shader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({
                            "model",
                            "view",
                            "projection",
                            "albedo",
                            "albedoMap",
                        }),
                        .textures = {
                            ::rmmr::resource::material::Asset::TextureBinding{
                                .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                                .texture = sky_sphere_pack.texture,
                            },
                        },
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });

        const auto manager = *with<Manager>::singleton(context);
        if (not with<Unit_group>::exists(context, manager)) {
            with<Unit_group>::extend(context, manager);
        }
        const auto sky_geometry_id = with<Unit_group>::addElement(
            context, manager,
            item<Unit>{.name = "skySphere_geometry", .library = "Eltanin"});
        with<geometry::Asset>::extend(context, sky_geometry_id, geometry::Asset::Quantum{});
        with<resources::SkySphereGenerator>::extend(context, sky_geometry_id, resources::SkySphereGenerator::Quantum{
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

        with<resources::SkySphereGenerator>::materialize(context, *assets.skySphereGeometry, window);

        with<scene::Interface>::createGrid(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Grid>{.geometry = *assets.primitive.grid, .material = *shared->material.grid, .opacity = 0.35f});

        const auto sky = with<scene::Interface>::createSimpleActor(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::actor::Simple>{
                .geometry = *assets.skySphereGeometry,
                .material = *assets.skySphereMaterial,
                .albedo = RGB{1.0f, 1.0f, 1.0f},
            });

        const Pos cameraPos{30.0f, 30.0f, 40.0f};
        const Pos cameraTarget{0.0f, 5.0f, 0.0f};
        const Pose cameraPose{.position = cameraPos, .rotation = glm::quatLookAt(glm::normalize(cameraTarget - cameraPos), vec3{0.0f, 1.0f, 0.0f})};
        const auto camera = with<scene::Interface>::createCamera(context, root, Locator{.pos = cameraPos, .euler = cameraPose.hpb()}, 100.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        // Temporary minimal spawn: mech k8 cell (visual: pack windowed_kube).
        (void)rmmr::necessary<::rmmr::resource::meshpack::Asset>(context, shared->primitives, "windowed_kube", [&](const auto& look) {
            return with<Block>::spawn(
                context,
                root,
                Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
                item<scene::actor::Mesh>{
                    .geometry = look.geometry,
                    .materials = look.materials,
                    .albedo = RGB{1.0f, 1.0f, 1.0f},
                    .scale = vec3{4.0f, 4.0f, 4.0f},
                });
        });

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

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
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
