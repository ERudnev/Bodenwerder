#include <tommy/invaders/session.h>

#include <tommy/invaders/bootstrap.h>
#include <tommy/world.h>

#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/scene/node.q1.h>

#include <GLFW/glfw3.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    namespace {

        using namespace api_for_internals;

        auto key_edge(const rmmr::system::Window::InputState& previous, const rmmr::system::Window::InputState& current, int key) -> bool {
            const auto down = [&](const vector<bool>& keys) {
                return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
            };
            return down(current.keys) and not down(previous.keys);
        }

        auto world_step(Reading context, World::Id world) -> integer {
            return with<World>::get(context, world).step;
        }

        auto is_menu_phase(Phase phase) -> bool {
            return phase == Phase::attract or phase == Phase::lost or phase == Phase::won;
        }

    } // namespace

    auto sessionPlaying(Reading context, Session::Id session) -> bool {
        if (not with<Session>::exists(context, session)) {
            return false;
        }
        return with<Session>::get(context, session).phase == Phase::playing;
    }

    auto Playfield::Actions::contains(Reading context, Id id, index2 point) -> bool {
        if (not with<Playfield>::exists(context, id)) {
            return false;
        }
        const auto& field = with<Playfield>::get(context, id);
        return point.x >= field.origin.x
            and point.x < field.origin.x + field.size.x
            and point.y >= field.origin.y
            and point.y < field.origin.y + field.size.y;
    }

    void Playfield::Actions::install(Writing context, Id id, index2 origin, index2 size) {
        auto field = with<Playfield>::modify(context, id);
        field->origin = origin;
        field->size = size;
    }

    void syncMenuCameraControl(Writing context, Session::Id session_id) {
        if (not with<Session>::exists(context, session_id)) {
            return;
        }
        const auto& session = with<Session>::get(context, session_id);
        const auto camera = session.camera;
        if (not with<rmmr::scene::Camera>::exists(context, camera)) {
            return;
        }

        if (is_menu_phase(session.phase)) {
            if (not with<rmmr::controller::Camera2d>::exists(context, camera)) {
                rmmr::controller::Camera2d::Actions::create(context, camera);
            }
            return;
        }

        if (with<rmmr::controller::Camera2d>::exists(context, camera)) {
            with<rmmr::controller::Camera2d>::remove(context, camera);
        }
        // Lock play view to origin after menu pan.
        auto node = with<rmmr::scene::Node>::modify(context, camera);
        node->position.x = 0.0f;
        node->position.y = 0.0f;
    }

    void noteFleetCleared(Writing context, Session::Id session_id) {
        auto session = with<Session>::modify(context, session_id);
        if (session->phase != Phase::playing) {
            return;
        }
        session->phase = Phase::wave_clear;
        session->wave_ready_at = world_step(context, session->world) + Session::wave_clear_steps;
        syncMenuCameraControl(context, session_id);
    }

    void notePlayerHit(Writing context, Session::Id session_id) {
        auto session = with<Session>::modify(context, session_id);
        if (session->phase != Phase::playing) {
            return;
        }
        session->lives -= 1;
        if (session->lives <= 0) {
            session->phase = Phase::lost;
            syncMenuCameraControl(context, session_id);
        }
    }

    struct Session::Internals : Session::DefaultInternals {
        // Doctrine !onWaveReady(~World) — whole object watch.
        static void onWaveReady(Reacting context) {
            const auto by_world = ask::relations<World>(context).updated<Session, &Session::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step) {
                    continue;
                }
                for (const auto session_id : by_world.ids(change.id)) {
                    auto session = with<Session>::modify(context, session_id);
                    if (session->phase != Phase::wave_clear) {
                        continue;
                    }
                    if (change.now.step < session->wave_ready_at) {
                        continue;
                    }
                    session->wave += 1;
                    session->phase = Phase::playing;
                    Bootstrap::installWave(context, session_id, session->wave);
                    syncMenuCameraControl(context, session_id);
                }
            }
        }

        // Doctrine !attractAndRestart(~World) — whole object watch (Enter / R).
        static void attractAndRestart(Reacting context) {
            const auto by_world = ask::relations<World>(context).updated<Session, &Session::Quantum::world>();
            for (const auto& change : context.changes<World>().updated()) {
                if (change.now.step <= change.old.step) {
                    continue;
                }
                for (const auto entry : context.proposal.aspect<rmmr::system::Window>().items()) {
                    const auto& input = with<rmmr::system::Window>::get(context, entry.id);
                    const bool enter = key_edge(input.previous, input.current, GLFW_KEY_ENTER);
                    const bool restart = key_edge(input.previous, input.current, GLFW_KEY_R);
                    if (not enter and not restart) {
                        continue;
                    }
                    for (const auto session_id : by_world.ids(change.id)) {
                        auto session = with<Session>::modify(context, session_id);
                        if (session->phase == Phase::attract and enter) {
                            session->phase = Phase::playing;
                            session->wave_ready_at = 0;
                            syncMenuCameraControl(context, session_id);
                        } else if ((session->phase == Phase::lost or session->phase == Phase::won) and restart) {
                            Bootstrap::resetMatch(context, session_id);
                        }
                    }
                }
            }
        }
    };

    auto Session::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::anchored<Session, World, &Session::Quantum::world>{},
            reaction::aspect_wide<Session, World>(&Session::Internals::onWaveReady),
            reaction::aspect_wide<Session, World>(&Session::Internals::attractAndRestart),
        };
    }

}
