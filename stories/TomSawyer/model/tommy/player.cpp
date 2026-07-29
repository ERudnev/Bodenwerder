#include <tommy/player.h>

#include <rmmr/scene/node.q1.h>
#include <rmmr/system/window.q1.h>

#include <GLFW/glfw3.h>

#include <cmath>

namespace tommy {

    using namespace fqsm::api;

    namespace {

        auto key_down(const vector<bool>& keys, int key) -> bool {
            return static_cast<std::size_t>(key) < keys.size() && keys[static_cast<std::size_t>(key)];
        }

        auto held_keys(Reading context) -> const rmmr::system::Window::InputState* {
            for (const auto entry : context->aspect<rmmr::system::Window>().items()) {
                return &with<rmmr::system::Window>::get(context, entry.id).current;
            }
            return nullptr;
        }

        // Flat2d: Node HPB.z rotates in XY. Local nose (+Y at bank=0) → world.
        auto nose_xy(float bank_degrees) -> rmmr::vec2 {
            const float radians = glm::radians(bank_degrees);
            return rmmr::vec2{-std::sin(radians), std::cos(radians)};
        }

    } // namespace

    void Player::Actions::applyThrusters(Writing context) {
        const auto* held = held_keys(context);
        if (not held) {
            return;
        }
        float turn = 0.0f;
        float throttle = 0.0f;
        if (key_down(held->keys, GLFW_KEY_A) or key_down(held->keys, GLFW_KEY_LEFT)) {
            turn += 1.0f;
        }
        if (key_down(held->keys, GLFW_KEY_D) or key_down(held->keys, GLFW_KEY_RIGHT)) {
            turn -= 1.0f;
        }
        if (key_down(held->keys, GLFW_KEY_W) or key_down(held->keys, GLFW_KEY_UP)) {
            throttle += 1.0f;
        }
        if (key_down(held->keys, GLFW_KEY_S) or key_down(held->keys, GLFW_KEY_DOWN)) {
            throttle -= 1.0f;
        }
        if (turn == 0.0f and throttle == 0.0f) {
            return;
        }
        for (const auto entry : context->aspect<Player>().items()) {
            const auto id = entry.id;
            if (not with<GameObject>::exists(context, id)) {
                continue;
            }
            const auto& object = with<GameObject>::get(context, id);
            if (not object.sprite) {
                continue;
            }
            if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
                continue;
            }
            const auto sprite = *object.sprite;
            if (turn != 0.0f) {
                auto hpb = with<rmmr::scene::Node>::hpb(context, sprite);
                hpb.z += turn * Player::turn_degrees_per_step;
                with<rmmr::scene::Node>::hpb(context, sprite, hpb);
            }
            if (throttle == 0.0f or not with<Inertia>::exists(context, id)) {
                continue;
            }
            const auto bank = with<rmmr::scene::Node>::hpb(context, sprite).z;
            const auto nose = nose_xy(bank);
            auto inertia = with<Inertia>::modify(context, id);
            inertia->vel.x += nose.x * throttle * Player::thrust_per_step;
            inertia->vel.y += nose.y * throttle * Player::thrust_per_step;
        }
    }

    void Player::Actions::tryFire(Writing context) {
        const auto* held = held_keys(context);
        if (not held) {
            return;
        }
        if (not key_down(held->keys, GLFW_KEY_SPACE)) {
            return;
        }
        for (const auto entry : context->aspect<Player>().items()) {
            const auto id = entry.id;
            const auto& player = with<Player>::get(context, id);
            with<Gun>::fire(context, player.gun, id);
        }
    }

    void Player::Actions::followCamera(Writing context) {
        for (const auto entry : context->aspect<Player>().items()) {
            const auto id = entry.id;
            if (not with<GameObject>::exists(context, id)) {
                continue;
            }
            const auto& object = with<GameObject>::get(context, id);
            if (not object.sprite) {
                continue;
            }
            if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
                continue;
            }
            const auto camera = with<Player>::get(context, id).camera;
            if (not with<rmmr::scene::Camera>::exists(context, camera)) {
                continue;
            }
            const auto& ship = with<rmmr::scene::Node>::get(context, *object.sprite);
            auto cam = with<rmmr::scene::Node>::modify(context, camera);
            cam->position.x = ship.position.x;
            cam->position.y = ship.position.y;
        }
    }

    struct Player::Internals : Player::DefaultInternals {};

    auto Player::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Player, Gun, &Player::Quantum::gun>{},
        };
    }

}
