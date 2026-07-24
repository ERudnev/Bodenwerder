#include "demos/kubeOfKubes.h"

#include <base/logging.h>
#include <rmmr/api/_interface.h>
#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/root.q1.h>

namespace toy::demos {

    using namespace fqsm::api;
    using namespace rmmr;

    void KubeOfKubes::seedAssets(Writing context, system::Core::Id core) {
        using namespace resource;
        using geometry::Generator;

        base::message("kubeOfKubes: seeding demo geometry...");

        assets.primitive.triangle = with<::rmmr::resource::Assets>::add_geometry_generator(
            context, core,
            item<Unit>{.manager = core, .name = "triangle", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::triangle});
        assets.primitive.kube = with<::rmmr::resource::Assets>::add_geometry_generator(
            context, core,
            item<Unit>{.manager = core, .name = "kube", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::kube});
        assets.primitive.bagel = with<::rmmr::resource::Assets>::add_geometry_generator(
            context, core,
            item<Unit>{.manager = core, .name = "bagel", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::bagel});
        assets.primitive.grid = with<::rmmr::resource::Assets>::add_geometry_generator(
            context, core,
            item<Unit>{.manager = core, .name = "grid", .library = "rmmr"},
            item<Generator>{.type = Generator::Type::gridPlane});
    }

    auto KubeOfKubes::setup(Writing context, const assets::Handles& shared) -> Handles {
        const auto root = with<scene::Interface>::createScene(context);

        constexpr int grid_extent = 4;
        constexpr float cube_edge = 1.0f;
        constexpr float spacing = cube_edge * 1.5f;
        const float center_offset = (static_cast<float>(grid_extent) - 1.0f) * 0.5f;
        const float cluster_lift = center_offset * spacing + cube_edge * 0.5f;

        for (int z = 0; z < grid_extent; ++z) {
            for (int y = 0; y < grid_extent; ++y) {
                for (int x = 0; x < grid_extent; ++x) {
                    const int cell = x + y + z;
                    const bool alpha_cutout = (cell % 5 == 0);
                    const Pos pos{
                        (static_cast<float>(x) - center_offset) * spacing,
                        (static_cast<float>(y) - center_offset) * spacing + cluster_lift,
                        (static_cast<float>(z) - center_offset) * spacing,
                    };
                    with<scene::Interface>::createSimpleActor(context, root,
                        Locator{
                            .pos = pos,
                            .euler = HPB{
                                -22.5f + 45.0f * static_cast<float>(x),
                                -15.0f + 30.0f * static_cast<float>(y),
                                -12.0f + 24.0f * static_cast<float>(z),
                            },
                        },
                        item<scene::actor::Simple>{
                            .geometry = (cell % 7 == 0) ? *assets.primitive.bagel : *assets.primitive.kube,
                            .material = alpha_cutout ? *shared.material.litTexturedAlpha : shared.material.debugLitTextured[cell % 4],
                            .albedo = RGB{
                                0.3f + 0.6f * static_cast<float>(x) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(y) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(z) / static_cast<float>(grid_extent - 1),
                            },
                        });
                }
            }
        }

        with<scene::Interface>::createGrid(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Grid>{.geometry = *assets.primitive.grid, .material = *shared.material.grid, .opacity = 1.0f});

        const auto camera = with<scene::Interface>::createCamera(context, root,
            Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{-18.0f, -36.0f, 0.0f}},
            item<scene::Camera>{.fov_y = 1.04719755f, .z_near = 0.1f, .z_far = 100.0f});
        with<controller::Camera>::create(context, camera);
        with<scene::Interface>::createLight(context, root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        return Handles{.scene = root, .camera = camera};
    }

}
