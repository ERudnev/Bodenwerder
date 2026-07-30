#include <tommy/world.q1.h>

#include <GLFW/glfw3.h>

namespace tommy {

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
        const integer add = static_cast<integer>(dt_us / k_us_per_step);
        for (const auto [id, _] : context->aspect<World>().items()) {
            if (with<World>::get(context, id).paused) {
                continue;
            }
            with<World>::modify(context, id)->step += add;
        }
    }

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
    };

    auto World::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<World, rmmr::system::Window>(&World::Internals::keyboardPauseKey),
        };
    }

}
