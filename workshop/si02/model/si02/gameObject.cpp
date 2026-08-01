#include <si02/gameObject.h>

#include <si02/shot.h>
#include <si02/world.h>

#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace si02 {

    using namespace fqsm::api;

    namespace {

        // Sprite world size: atlas texels × Sprite.scale (see sprite.vert.glsl).
        // Circle diameter = longer atlas side × uniform scale → radius matches drawn sprite.
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
            const float diameter = std::max(width, height) * scale;
            return 0.5f * diameter;
        }

        auto world_step(Reading context) -> integer {
            for (const auto entry : context->aspect<World>().items()) {
                return with<World>::get(context, entry.id).step;
            }
            return 0;
        }

    } // namespace

    auto GameObject::customAspectReactions() -> const Behavior {
        return {};
    }

    void GameObject::Actions::destroy(Writing context, Id body) {
        if (not with<GameObject>::exists(context, body)) {
            return;
        }
        const auto sprite = with<GameObject>::get(context, body).sprite;
        // Drop Node (Sprite Feature goes with remove_with_parent). Then drop GameObject
        // (parasitic Stone/Player/Shot/Physical/Inertia/AnimatedDecay follow).
        if (sprite and with<rmmr::scene::Node>::exists(context, *sprite)) {
            with<rmmr::scene::Node>::remove(context, *sprite);
        }
        with<GameObject>::remove(context, body);
    }

    void AnimatedDecay::Actions::grantToDepleted(Writing context) {
        const integer now = world_step(context);
        for (const auto entry : context->aspect<Physical>().items()) {
            const auto id = entry.id;
            if (with<Physical>::get(context, id).hitpoints > 0) {
                continue;
            }
            if (with<AnimatedDecay>::exists(context, id)) {
                continue;
            }
            if (not with<GameObject>::exists(context, id)) {
                continue;
            }
            float initial_opacity = 1.0f;
            const auto& object = with<GameObject>::get(context, id);
            if (object.sprite and with<rmmr::scene::actor::Sprite>::exists(context, *object.sprite)) {
                initial_opacity = with<rmmr::scene::actor::Sprite>::get(context, *object.sprite).opacity;
            }
            with<AnimatedDecay>::extend(context, id, AnimatedDecay::Quantum{
                .born_at = now,
                .initial_opacity = initial_opacity,
            });
        }
    }

    void AnimatedDecay::Actions::update(Writing context) {
        const integer now = world_step(context);
        vector<GameObject::Id> expired;
        for (const auto entry : context->aspect<AnimatedDecay>().items()) {
            const auto id = entry.id;
            if (not with<GameObject>::exists(context, id)) {
                continue;
            }
            const auto& decay = with<AnimatedDecay>::get(context, id);
            const integer age = now - decay.born_at;
            const float t = AnimatedDecay::duration_steps > 0
                ? std::clamp(
                    static_cast<float>(age) / static_cast<float>(AnimatedDecay::duration_steps),
                    0.0f,
                    1.0f)
                : 1.0f;
            const float opacity = decay.initial_opacity * (1.0f - t);
            const auto& object = with<GameObject>::get(context, id);
            if (object.sprite and with<rmmr::scene::actor::Sprite>::exists(context, *object.sprite)) {
                with<rmmr::scene::actor::Sprite>::modify(context, *object.sprite)->opacity = opacity;
            }
            if (age >= AnimatedDecay::duration_steps) {
                expired.push_back(id);
            }
        }
        for (const auto id : expired) {
            with<GameObject>::destroy(context, id);
        }
    }

    void Physical::Actions::resolveCollisions(Writing context) {
        struct Body {
            Id id;
            rmmr::scene::Node::Id node;
            float radius = 0.0f;
            float mass = 1.0f;
        };
        vector<Body> bodies;
        for (const auto entry : context->aspect<Physical>().items()) {
            const auto id = entry.id;
            if (with<Shot>::exists(context, id) or with<AnimatedDecay>::exists(context, id)) {
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
            const auto& physical = with<Physical>::get(context, id);
            bodies.push_back(Body{
                .id = id,
                .node = *object.sprite,
                .radius = collider_radius(context, *object.sprite),
                .mass = physical.mass > 0.0f ? physical.mass : physical.size,
            });
        }
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                auto node_a = with<rmmr::scene::Node>::modify(context, bodies[i].node);
                auto node_b = with<rmmr::scene::Node>::modify(context, bodies[j].node);
                const float dx = node_b->position.x - node_a->position.x;
                const float dy = node_b->position.y - node_a->position.y;
                const float dist_sq = dx * dx + dy * dy;
                const float min_dist = bodies[i].radius + bodies[j].radius;
                if (min_dist <= 0.0f) {
                    continue;
                }
                if (dist_sq >= min_dist * min_dist) {
                    continue;
                }
                const float dist = dist_sq > 0.0f ? std::sqrt(dist_sq) : 0.0f;
                const float nx = dist > 0.0f ? dx / dist : 1.0f;
                const float ny = dist > 0.0f ? dy / dist : 0.0f;
                const float overlap = min_dist - dist;
                const float mass_sum = bodies[i].mass + bodies[j].mass;
                const float share_a = bodies[j].mass / mass_sum;
                const float share_b = bodies[i].mass / mass_sum;
                node_a->position.x -= nx * overlap * share_a;
                node_a->position.y -= ny * overlap * share_a;
                node_b->position.x += nx * overlap * share_b;
                node_b->position.y += ny * overlap * share_b;
            }
        }
    }

    void Inertia::Actions::update(Writing context) {
        // One World.step unit. Source of vel is outside this aspect.
        constexpr float dt = 1.0f;
        for (const auto entry : context->aspect<Inertia>().items()) {
            const auto id = entry.id;
            if (with<AnimatedDecay>::exists(context, id)) {
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
            auto inertia = with<Inertia>::modify(context, id);
            auto node = with<rmmr::scene::Node>::modify(context, *object.sprite);
            node->pose.position += inertia->vel * dt;
            inertia->vel *= (1.0f - inertia->saturation);
        }
    }

}
