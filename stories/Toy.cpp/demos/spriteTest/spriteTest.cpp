#include "demos/spriteTest/spriteTest.h"

#include <base/logging.h>
#include <imgui.h>
#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>

#include "demos/spriteTest/model.h"

namespace sprdemo {

    using namespace fqsm::api;
    using namespace rmmr;

    Schema SpriteTest::schema() const {
        return ask::schema::aspect<God>();
    }

    void SpriteTest::addAssets(Writing context, system::Core::Id core) {
        using namespace resource;
        using geometry::Generator;

        base::message("spriteTest: seeding Kenney sprite pack...");
        assets.kenney = with<::rmmr::resource::Assets>::add_sprites_kenney(
            context,
            core,
            item<Unit>{.manager = core, .name = "space_shooter_kenney", .library = "Toy"},
            item<sprite::LoaderKenney>{
                .image = "sprites/Spritesheet/sheet.png",
                .descriptor = "sprites/Spritesheet/sheet.xml",
            });

        base::message("spriteTest: seeding unit quad geometry...");
        assets.unitQuad = with<::rmmr::resource::Assets>::add_geometry_generator(
            context,
            core,
            item<Unit>{.manager = core, .name = "sprite_unit_quad", .library = "Toy"},
            item<Generator>{.type = Generator::Type::unitQuad});
    }

    void SpriteTest::setup(Writing context, system::Core::Id, system::Viewport::Id viewport) {
        const auto root = with<scene::Interface>::createScene(context);

        with<scene::Flat2d>::extend(context, root, scene::Flat2d::Quantum{
            .size = index2{1600, 900},
        });
        with<scene::actor::Sprite>::modify_global(context)->geometry = *assets.unitQuad;

        const auto spawn = [&](index2 pos, integer zet, integer index, float scale) {
            with<scene::Flat2d>::createSpriteActor(context, root,
                Locator{
                    .pos = Pos{
                        static_cast<float>(pos.x),
                        static_cast<float>(pos.y),
                        static_cast<float>(zet),
                    },
                    .euler = HPB{0.0f, 0.0f, 0.0f},
                },
                item<scene::actor::Sprite>{
                    .material = *shared->material.sprite,
                    .tint = RGB{0.0f, 0.0f, 0.0f},
                    .scale = vec3{scale, scale, 1.0f},
                    .pack = *assets.kenney,
                    .index = index,
                });
        };

        constexpr integer grid = 10;
        constexpr integer step = 100;
        for (integer row = 0; row < grid; ++row) {
            for (integer col = 0; col < grid; ++col) {
                spawn(index2{col * step, row * step}, 0, row * grid + col, 1.0f);
            }
        }

        const auto camera = with<scene::Flat2d>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}});
        with<controller::Camera2d>::create(context, camera);

        views = {
            View{.viewport = viewport, .scene = root, .camera = camera},
        };
    }

    void SpriteTest::contributeViewMenu() {
        ImGui::MenuItem("Camera", nullptr, &ui.camera);
    }

    void SpriteTest::drawUi(Writing world) {
        drawCameraWindow(world);
    }

    void SpriteTest::drawCameraWindow(Writing world) {
        if (not ui.camera or views.empty())
            return;

        bool open = ui.camera;
        if (ImGui::Begin("Camera", &open)) {
            const auto camera = views.front().camera;
            if (not with<scene::Camera>::exists(world, camera)) {
                ImGui::TextDisabled("No camera selected.");
            } else {
                auto quantum = with<scene::Camera>::modify(world, camera);
                if (quantum->mode == scene::Camera::Mode::orthographic) {
                    int size[2] = {quantum->ortho_size.x, quantum->ortho_size.y};
                    if (ImGui::DragInt2("Ortho size", size, 1.0f, 1, 8192))
                        quantum->ortho_size = index2{size[0], size[1]};
                } else if (quantum->mode == scene::Camera::Mode::perspective) {
                    ImGui::SliderAngle("FoV", &quantum->fov_y, 10.0f, 160.0f);
                } else {
                    ImGui::TextDisabled("Parallel projection (reserved).");
                }
                ImGui::DragFloat("Near", &quantum->z_near, 0.01f, 0.001f, quantum->z_far - 0.001f, "%.3f");
                ImGui::DragFloat("Far", &quantum->z_far, 0.1f, quantum->z_near + 0.001f, 10000.0f, "%.3f");
            }
        }
        ImGui::End();
        ui.camera = open;
    }

}
