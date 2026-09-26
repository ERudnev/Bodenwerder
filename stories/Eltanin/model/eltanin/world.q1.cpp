#include <eltanin/world.q1.h>

#include "geo/celestial/sun.h"
#include <rmmr/scene/node.q1.h>

#include <GLFW/glfw3.h>

namespace eltanin {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        constexpr int k_pause_key = GLFW_KEY_P;
        constexpr int64 k_us_per_step = 1000; // 1000 steps/sec from wall μs

    } // namespace

    void World::Actions::advance(Writing context, int64 dt_us) {
        if (dt_us < k_us_per_step) {
            return;
        }
        if (with<World>::get_global(context).paused) {
            return;
        }
        with<World>::modify_global(context)->step += static_cast<integer>(dt_us / k_us_per_step);
    }

    void World::Actions::placeCamera(Writing context, rmmr::Pose pose) {
        const auto camera = with<World>::get_global(context).camera;
        if (not camera)
            return (void)context.refuse("eltanin::World::placeCamera: camera missing");
        with<rmmr::scene::Node>::modify(context, *camera)->pose = pose;
    }

    void World::Actions::tetherEnvironment(Writing context) {
        const auto& global = with<World>::get_global(context);
        if (not global.sky || not global.camera) {
            return;
        }
        if (not with<rmmr::scene::Node>::exists(context, *global.sky)) {
            return;
        }
        if (not with<rmmr::scene::Node>::exists(context, *global.camera)) {
            return;
        }
        with<rmmr::scene::Node>::modify(context, *global.sky)->pose.position =
            with<rmmr::scene::Node>::get(context, *global.camera).pose.position;
        if (global.skyBackdrop and with<rmmr::scene::Node>::exists(context, *global.skyBackdrop))
            with<rmmr::scene::Node>::modify(context, *global.skyBackdrop)->pose.position =
                with<rmmr::scene::Node>::get(context, *global.camera).pose.position;
        geo::Sun::tether(context, with<rmmr::scene::Node>::get(context, *global.camera).pose.position);
    }

    // Once per frame: the Window keeps the previous and the current input snapshot, so the edge is visible without a delta.
    void World::Actions::pollPauseKey(Writing context) {
        const auto bound = with<World>::get_global(context).window;
        if (not bound or not with<rmmr::system::Window>::exists(context, *bound))
            return;
        const auto& window = with<rmmr::system::Window>::get(context, *bound);
        const bool was_down = key_down(window.previous.keys, k_pause_key);
        const bool is_down = key_down(window.current.keys, k_pause_key);
        if (was_down or not is_down)
            return;
        auto world = with<World>::modify_global(context);
        world->paused = not world->paused;
    }

    auto doctrine::world() -> Schema {
        return ask::schema::aspect<World>();
    }

}
