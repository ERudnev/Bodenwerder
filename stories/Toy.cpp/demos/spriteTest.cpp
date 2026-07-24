#include "demos/spriteTest.h"

#include <rmmr/scene/root.q1.h>

namespace toy::demos {

    using namespace fqsm::api;
    using namespace rmmr;

    void SpriteTest::seedAssets(Writing, system::Core::Id, const assets::Handles&) {
        // Sprite pack / materials land here later; fills `assets` using shared as base.
    }

    auto SpriteTest::setup(Writing context, const assets::Handles&) -> Handles {
        const auto root = with<scene::Interface>::createScene(context);
        const auto camera = with<scene::Interface>::createCamera(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 5.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Camera>{.fov_y = 1.04719755f, .z_near = 0.1f, .z_far = 100.0f});
        return Handles{.scene = root, .camera = camera};
    }

}
