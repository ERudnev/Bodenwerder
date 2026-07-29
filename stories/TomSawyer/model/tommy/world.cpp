#include <tommy/world.h>

#include <tommy/gameObject.h>
#include <tommy/player.h>
#include <tommy/shot.h>

#include <GLFW/glfw3.h>

namespace tommy {

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
            for (const auto& change : context.changes<rmmr::system::Window>().updated()) {
                const bool was_down = key_down(change.old.current.keys, k_pause_key);
                const bool is_down = key_down(change.now.current.keys, k_pause_key);
                if (was_down || not is_down) {
                    continue;
                }
                for (const auto [id, _] : context.proposal.aspect<World>().items()) {
                    auto world = with<World>::modify(context, id);
                    world->paused = not world->paused;
                }
            }
        }

        // Clock μs → World.step. Then per step: thrusters → inertia; then collisions once.
        static void advanceStep(Reacting context) {
            integer steps_advanced = 0;
            for (const auto& change : context.changes<rmmr::system::Clock>().updated()) {
                const int64 dt_us = change.now.absolute - change.old.absolute;
                if (dt_us < k_us_per_step) {
                    continue;
                }
                const integer add = static_cast<integer>(dt_us / k_us_per_step);
                for (const auto [id, _] : context.proposal.aspect<World>().items()) {
                    if (with<World>::get(context, id).paused) {
                        continue;
                    }
                    with<World>::modify(context, id)->step += add;
                    steps_advanced += add;
                }
            }
            for (integer step = 0; step < steps_advanced; ++step) {
                with<Player>::applyThrusters(context);
                with<Player>::tryFire(context);
                with<Inertia>::update(context);
            }
            if (steps_advanced > 0) {
                with<Shot>::resolveHits(context);
                with<Shot>::cullExpired(context);
                with<Physical>::resolveCollisions(context);
                with<Player>::followCamera(context);
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
