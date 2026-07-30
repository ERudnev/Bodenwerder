#include <kubes/world.h>

#include <GLFW/glfw3.h>

namespace kubes {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        constexpr int k_pause_key = GLFW_KEY_P;
        constexpr int64 k_us_per_step = 1000; // 1000 steps/sec from Core μs

    } // namespace

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

        // Clock μs → World Global.step. Write only when step grows so ~World gameplay stays quiet otherwise.
        static void advanceStep(Reacting context) {
            for (const auto& change : context.changes<rmmr::system::Clock>().updated()) {
                const int64 dt_us = change.now.absolute - change.old.absolute;
                if (dt_us < k_us_per_step) {
                    continue;
                }
                if (with<World>::get_global(context).paused) {
                    continue;
                }
                const integer add = static_cast<integer>(dt_us / k_us_per_step);
                with<World>::modify_global(context)->step += add;
            }
        }
    };

    auto World::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<World, rmmr::system::Window>(&World::Internals::keyboardPauseKey),
            reaction::aspect_wide<World, rmmr::system::Clock>(&World::Internals::advanceStep),
        };
    }

}
