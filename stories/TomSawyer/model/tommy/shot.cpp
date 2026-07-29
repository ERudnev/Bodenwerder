#include <tommy/shot.h>

#include <tommy/player.h>
#include <tommy/sun.h>
#include <tommy/world.h>

#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tommy {

    using namespace fqsm::api;

    namespace {

        auto collider_radius(Reading context, rmmr::scene::actor::Sprite::Id sprite_id) -> float {
            if (not with<rmmr::scene::actor::Sprite>::exists(context, sprite_id)) {
                return 0.0f;
            }
            const auto& sprite = with<rmmr::scene::actor::Sprite>::get(context, sprite_id);
            if (not with<rmmr::resource::sprite::Pack>::exists(context, sprite.pack)) {
                return 0.0f;
            }
            const auto& pack = with<rmmr::resource::sprite::Pack>::get(context, sprite.pack);
            if (sprite.index < 0 or static_cast<std::size_t>(sprite.index) >= pack.entries.size()) {
                return 0.0f;
            }
            const auto& entry = pack.entries[static_cast<std::size_t>(sprite.index)];
            const float width = static_cast<float>(entry.max.x - entry.min.x);
            const float height = static_cast<float>(entry.max.y - entry.min.y);
            const float scale = std::max(std::abs(sprite.scale.x), std::abs(sprite.scale.y));
            return 0.5f * std::max(width, height) * scale;
        }

    } // namespace

    void Shot::Actions::resolveHits(Writing context) {
        struct Target {
            GameObject::Id id;
            rmmr::scene::Node::Id node;
            float radius = 0.0f;
        };
        vector<Target> targets;
        for (const auto entry : context->aspect<Physical>().items()) {
            const auto id = entry.id;
            if (with<Shot>::exists(context, id)
                or with<Player>::exists(context, id)
                or with<Sun>::exists(context, id)
                or with<AnimatedDecay>::exists(context, id))
            {
                continue;
            }
            if (with<Physical>::get(context, id).hitpoints <= 0) {
                continue;
            }
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
            targets.push_back(Target{
                .id = id,
                .node = *object.sprite,
                .radius = collider_radius(context, *object.sprite),
            });
        }

        vector<GameObject::Id> spent_bolts;
        for (const auto entry : context->aspect<Shot>().items()) {
            const auto shot_id = entry.id;
            if (not with<GameObject>::exists(context, shot_id)) {
                continue;
            }
            const auto& object = with<GameObject>::get(context, shot_id);
            if (not object.sprite) {
                continue;
            }
            if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
                continue;
            }
            const auto& shot_node = with<rmmr::scene::Node>::get(context, *object.sprite);
            const float shot_radius = collider_radius(context, *object.sprite);
            for (const auto& target : targets) {
                if (not with<Physical>::exists(context, target.id)) {
                    continue;
                }
                const auto& target_node = with<rmmr::scene::Node>::get(context, target.node);
                const float dx = target_node.position.x - shot_node.position.x;
                const float dy = target_node.position.y - shot_node.position.y;
                const float min_dist = shot_radius + target.radius;
                if (min_dist <= 0.0f) {
                    continue;
                }
                if (dx * dx + dy * dy >= min_dist * min_dist) {
                    continue;
                }
                auto physical = with<Physical>::modify(context, target.id);
                physical->hitpoints = std::max(integer{0}, physical->hitpoints - Shot::damage);
                spent_bolts.push_back(shot_id);
                break;
            }
        }

        for (const auto id : spent_bolts) {
            with<GameObject>::destroy(context, id);
        }
    }

    void Shot::Actions::cullExpired(Writing context) {
        integer now = 0;
        for (const auto entry : context->aspect<World>().items()) {
            now = with<World>::get(context, entry.id).step;
            break;
        }
        vector<GameObject::Id> expired;
        for (const auto entry : context->aspect<Shot>().items()) {
            if (with<Shot>::get(context, entry.id).expires_at <= now) {
                expired.push_back(entry.id);
            }
        }
        for (const auto id : expired) {
            with<GameObject>::destroy(context, id);
        }
    }

}
