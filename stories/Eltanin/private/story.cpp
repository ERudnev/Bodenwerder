#include "story.h"

#include <eltanin/entities/block.q1.h>
#include <eltanin/geo/boulder.q1.h>
#include <eltanin/geo/rock.q1.h>
#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/clast.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <eltanin/physics/strong.q1.h>
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
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/viewport.q1.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema Game::schema() const {
        return ask::schema::merge({
            ask::schema::aspect<World>(),
            ask::schema::aspect<phys::Particle>(),
            ask::schema::aspect<phys::Atomic>(),
            ask::schema::aspect<phys::Clast>(),
            ask::schema::aspect<phys::strong::Nail>(),
            ask::schema::aspect<phys::strong::Gluon>(),
            ask::schema::aspect<Block>(),
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

        if (not shared or not shared->material.lit) {
            return (void)context.refuse("eltanin::Game::addAssets: rmmr lit missing");
        }
        const auto rock_shader = with<Assets>::add_shader_loader(
            context,
            Name::from("Eltanin", "rock"),
            item<shader::Loader>{
                .vertex = "shaders/rock.vert.glsl",
                .fragment = "shaders/rock.frag.glsl",
            });
        const auto& litQuantum = with<Material>::get(context, *shared->material.lit);
        const auto shadowTechnique = litQuantum.techniques.find(renderer::Pass::shadow);
        if (shadowTechnique == litQuantum.techniques.end()) {
            return (void)context.refuse("eltanin::Game::addAssets: lit shadow technique missing");
        }
        assets.rockMaterial = with<Assets>::add_material(
            context,
            Name::from("Eltanin", "rock"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::opaque, Material::Technique{
                        .program = with<Unit>::remember(context, rock_shader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}),
                    }},
                    {renderer::Pass::shadow, Material::Technique{
                        .program = shadowTechnique->second.program,
                        .uniforms = {},
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::inherit,
            });

        const auto boulder_shader = with<Assets>::add_shader_loader(
            context,
            Name::from("Eltanin", "boulder"),
            item<shader::Loader>{
                .vertex = "shaders/boulder.vert.glsl",
                .fragment = "shaders/boulder.frag.glsl",
            });
        assets.boulderMaterial = with<Assets>::add_material(
            context,
            Name::from("Eltanin", "boulder"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::opaque, Material::Technique{
                        .program = with<Unit>::remember(context, boulder_shader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}),
                    }},
                    {renderer::Pass::shadow, Material::Technique{
                        .program = shadowTechnique->second.program,
                        .uniforms = {},
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::inherit,
            });

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
        const auto crust_id = with<Unit_group>::addElement(context, manager, Unit::Quantum{.name = Name::from("Eltanin", "crust")});
        with<texture3array::Asset>::extend(context, crust_id, texture3array::Asset::Quantum{.layerSize = index3{0, 0, 0}, .capacity = 0});
        assets.crust = crust_id;
    }

    void Game::prepareAssets(Writing) {
    }

    void Game::populateWorld(Writing context, system::Window::Id window) {
        {
            auto world = with<World>::modify_global(context);
            world->window = window;
            world->paused = true;
        }

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

        const Pos cameraPos{0.0f, 160.0f, 520.0f};
        const Pos cameraTarget{0.0f, 0.0f, 0.0f};
        const Pose cameraPose{.position = cameraPos, .rotation = glm::quatLookAt(glm::normalize(cameraTarget - cameraPos), vec3{0.0f, 1.0f, 0.0f})};
        const auto camera = with<scene::Interface>::createCamera(context, root, cameraPose, 100.0f * std::numbers::pi_v<float> / 180.0f);
        with<controller::Camera3d>::create(context, camera);
        with<scene::Interface>::createLight(context, root, Pose::from(Pos{160.0f, 280.0f, 120.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 8.0f, .range = 1600.0f});

        // with<geo::Rock>::spawnIceSphere(context, root, window, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
        // with<geo::Rock>::spawnPaletteTorus(context, root, window, Pose::from(Pos{80.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));

        auto nibble = [](int channel, int fill) -> geo::Mix { return geo::Mix{static_cast<std::uint64_t>(fill)} << (channel * 4); };
        const geo::Mix palettes[10]{
            nibble(0, 15),
            nibble(1, 8) | nibble(3, 7),
            nibble(1, 10) | nibble(2, 4) | nibble(3, 1),
            nibble(1, 6) | nibble(2, 3) | nibble(6, 4) | nibble(7, 2),
            nibble(5, 11) | nibble(4, 3) | nibble(1, 1),
            nibble(6, 9) | nibble(7, 5) | nibble(8, 1),
            nibble(0, 8) | nibble(1, 4) | nibble(14, 3),
            nibble(3, 7) | nibble(4, 4) | nibble(9, 4),
            nibble(2, 10) | nibble(11, 3) | nibble(6, 2),
            nibble(5, 8) | nibble(0, 5) | nibble(15, 2),
        };
        auto circularVelocity = [](vec3 position) -> vec3 {
            const float radius = glm::length(position);
            if (radius < 1.0f)
                return vec3{0.0f, 0.0f, 0.0f};
            vec3 tangent = glm::cross(vec3{0.0f, 1.0f, 0.0f}, position);
            if (glm::dot(tangent, tangent) < 1.0e-8f)
                tangent = glm::cross(vec3{1.0f, 0.0f, 0.0f}, position);
            return glm::normalize(tangent) * std::sqrt(phys::Settings::centralMu / radius);
        };
        std::mt19937 rng{20260817};
        std::normal_distribution<float> gauss{0.0f, 1.0f};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        constexpr float goldenAzim = 137.508f;
        constexpr float periodMin = 2.5f;
        constexpr float periodMax = 60.0f;
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        for (int index = 0; index < 50; ++index) {
            const float diameter = (index < 2) ? 100.0f : 12.0f + 13.0f * unit(rng);
            const float period = periodMin * std::pow(periodMax / periodMin, unit(rng));
            const float orbitKepler = std::cbrt(phys::Settings::centralMu * period * period / (twoPi * twoPi));
            const float orbit = glm::max(orbitKepler, diameter * 0.55f + 40.0f);
            const float azim = (goldenAzim * static_cast<float>(index) + 8.0f * gauss(rng)) * std::numbers::pi_v<float> / 180.0f;
            const Pose pose = Pose::from(Pos{orbit * std::cos(azim), 0.0f, orbit * std::sin(azim)}, HPB{360.0f * unit(rng), 30.0f * gauss(rng), 360.0f * unit(rng)});
            const geo::Recipe recipe{
                .mix = palettes[index % 10],
                .spotMeters = glm::clamp(diameter * 0.22f, 4.0f, 28.0f),
                .spotContrast = glm::clamp(0.35f + 0.25f * gauss(rng), 0.05f, 0.90f),
                .diameterMeters = diameter,
                .lump = glm::clamp(0.35f + 0.20f * gauss(rng), 0.12f, 0.80f),
                .seed = 1100 + index,
            };
            const vec3 omega{0.35f * gauss(rng), 0.55f * gauss(rng), 0.35f * gauss(rng)};
            with<geo::Rock>::spawnGenerated(context, root, window, pose, recipe, circularVelocity(pose.position), omega);
        }

        std::mt19937 debrisRng{20260818};
        for (int index = 0; index < 220; ++index) {
            const float diameter = 0.5f + 3.5f * unit(debrisRng);
            const float period = periodMin * std::pow(periodMax / periodMin, unit(debrisRng));
            const float orbitKepler = std::cbrt(phys::Settings::centralMu * period * period / (twoPi * twoPi));
            const float orbit = glm::max(orbitKepler, diameter * 0.55f + 40.0f);
            const float azim = (goldenAzim * static_cast<float>(index) + 41.0f + 6.0f * gauss(debrisRng)) * std::numbers::pi_v<float> / 180.0f;
            const Pose pose = Pose::from(Pos{orbit * std::cos(azim), 0.0f, orbit * std::sin(azim)}, HPB{360.0f * unit(debrisRng), 30.0f * gauss(debrisRng), 360.0f * unit(debrisRng)});
            const geo::Boulder::Recipe recipe{
                .mineral = static_cast<integer>(index % 16),
                .diameterMeters = diameter,
                .lump = glm::clamp(0.40f + 0.25f * gauss(debrisRng), 0.15f, 1.0f),
                .seed = 4100 + index,
            };
            const vec3 omega{0.55f * gauss(debrisRng), 0.75f * gauss(debrisRng), 0.55f * gauss(debrisRng)};
            with<geo::Boulder>::spawn(context, root, window, pose, recipe, circularVelocity(pose.position), omega);
        }

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
        mountPack.bind(with<::rmmr::resource::Manager>::get(context, *manager).location / "Eltanin" / "fittings" / "mounts");
        blueprints.create(context);
    }

    void Game::setup(Writing context, system::Window::Id window) {
        populateWorld(context, window);
    }

    void Game::advanceSim(Writing context, int64 dt_us) {
        with<World>::advance(context, dt_us);
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
        const int64 simDt = with<World>::get_global(world).paused ? int64{0} : dt_us;
        physics.step(world, simDt);
        advanceSim(world, simDt);
    }

}
