#include "story.h"

#include <kubes/resources/geometry.h>
#include <kubes/world.h>
#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/viewport.q1.h>

#include <numbers>

namespace kubes {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema KubeOfKubes::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<resources::SkySphereGenerator>(),
        });
    }

    void KubeOfKubes::addAssets(Writing context, system::Core::Id core) {
        using namespace resource;
        using geometry::Generator;

        assets.primitive.grid = with<::rmmr::resource::Assets>::add_geometry_generator(
            context, core,
            item<Unit>{.manager = core, .name = "grid", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::gridPlane});

        assets.skySphere = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context, core,
            item<Unit>{.manager = core, .name = "skySphere", .library = "Kubes"},
            item<sprite::LoaderKenney>{
                .image = "sprites/skySphere.png",
                .descriptor = "sprites/skySphere.xml",
            });

        const auto sky_sphere_shader = with<::rmmr::resource::Assets>::add_shader_loader(
            context, core,
            item<Unit>{.manager = core, .name = "skySphere_shader", .library = "Kubes"},
            item<shader::Loader>{
                .vertex = "shaders/skySphere.vert.glsl",
                .fragment = "shaders/skySphere.frag.glsl",
            });

        const auto& sky_sphere_pack = with<sprite::Pack>::get(context, *assets.skySphere);
        assets.skySphereMaterial = with<::rmmr::resource::Assets>::add_material(
            context, core,
            item<Unit>{.manager = core, .name = "skySphere_material", .library = "Kubes"},
            resource::material::Asset::Quantum{
                .techniques = {
                    {renderer::Pass::environment, resource::material::Asset::Technique{
                        .program = with<Unit>::remember(context, sky_sphere_shader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({
                            "model",
                            "view",
                            "projection",
                            "albedo",
                            "albedoMap",
                        }),
                        .textures = {
                            resource::material::Asset::TextureBinding{
                                .uniform = ::rmmr::material::Semantics::id_of("albedoMap"),
                                .texture = sky_sphere_pack.texture,
                            },
                        },
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::additive,
            });

        if (not with<Unit_group>::exists(context, core)) {
            with<Unit_group>::extend(context, core);
        }
        const auto sky_geometry_id = with<Unit_group>::addElement(
            context, core,
            item<Unit>{.manager = core, .name = "skySphere_geometry", .library = "Kubes"});
        with<geometry::Asset>::extend(context, sky_geometry_id, geometry::Asset::Quantum{});
        with<resources::SkySphereGenerator>::extend(context, sky_geometry_id, resources::SkySphereGenerator::Quantum{
            .count = 48800, // 20k×2, then halo ×2 again (~31k disk + ~18k halo)
            .seed = 1,
            .angular_diameter_deg = 0.41f,
        });
        assets.skySphereGeometry = sky_geometry_id;
    }

    void KubeOfKubes::populateWorld(Writing context, system::Window::Id window) {
        with<World>::modify_global(context)->window = window;

        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.0f, 0.0f, 0.0f, 1.0f},
        });

        const auto root = with<scene::Interface>::createScene(context);

        resources::SkySphereGenerator::Actions::materialize(context, *assets.skySphereGeometry, window);

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

        const auto camera = with<scene::Interface>::createCamera(context, root,
            Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{36.87f, -29.74f, 0.0f}},
            100.0f * std::numbers::pi_v<float> / 180.0f);
        // R=100 sphere + offset camera: default z_far=100 would clip the far hemisphere.
        with<scene::Camera>::modify(context, camera)->z_far = 250.0f;
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

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

    void KubeOfKubes::setup(establish::Realm& world, system::Core::Id, system::Window::Id window) {
        populateWorld(world, window);
    }

    void KubeOfKubes::advanceSim(Writing context, int64 dt_us) {
        with<World>::advance(context, dt_us);
    }

    void KubeOfKubes::onFrame(establish::Realm& world, int64 dt_us) {
        with<World>::tetherEnvironment(world);
        advanceSim(world, dt_us);
    }

}
