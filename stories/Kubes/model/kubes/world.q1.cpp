#include <kubes/world.q1.h>

#include <rmmr/scene/node.q1.h>

#include <GLFW/glfw3.h>

namespace kubes {

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
        with<rmmr::scene::Node>::modify(context, *global.sky)->position =
            with<rmmr::scene::Node>::get(context, *global.camera).position;
    }

    struct World::Internals : World::DefaultInternals {
        static void keyboardPauseKey(Reacting context) {
            const auto bound = with<World>::get_global(context).window;
            if (not bound) {
                return;
            }
            for (const auto& change : context.changes<rmmr::system::Window>().updated()) {
                if (change.id != *bound) {
                    continue;
                }
                const bool was_down = key_down(change.old.current.keys, k_pause_key);
                const bool is_down = key_down(change.now.current.keys, k_pause_key);
                if (was_down || not is_down) {
                    continue;
                }
                auto world = with<World>::modify_global(context);
                world->paused = not world->paused;
            }
        }
    };

    auto World::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<World, rmmr::system::Window>(&World::Internals::keyboardPauseKey),
        };
    }

}
