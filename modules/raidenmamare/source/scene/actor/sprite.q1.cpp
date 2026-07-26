#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/submit.h>

#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    auto Sprite::Actions::create(
        Writing context,
        Pos position,
        HPB hpb,
        vec3 scale,
        resource::material::Asset::Id material,
        RGB tint,
        resource::sprite::Pack::Id pack,
        integer index) -> Id
    {
        const auto node = Node::Actions::create(context, Locator{
            .pos = position,
            .euler = hpb,
        });
        with<Sprite>::extend(context, node, Sprite::Quantum{
            .material = material,
            .tint = tint,
            .scale = scale,
            .pack = pack,
            .index = index,
        });
        return node;
    }

    void Sprite::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& actor = with<Sprite>::get(context, node);
        const auto& global = with<Sprite>::get_global(context);
        if (not global.geometry) {
            return;
        }

        auto model = Node::Actions::transform(context, node);
        model = glm::scale(model, actor.scale);
        submit_material_passes(context, device, DrawInstance{
            .model = model,
            .geometry = *global.geometry,
            .material = actor.material,
            .sprite = DrawInstance::SpriteSource{
                .pack = actor.pack,
                .index = actor.index,
            },
            .albedo = RGB{1.0f, 1.0f, 1.0f} + actor.tint,
            .opacity = 1.0f,
        }, where);
    }

}
