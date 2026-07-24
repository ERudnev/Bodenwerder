#include <rmmr/scene/actors/sprite.q1.h>

#include <base/logging.h>

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

    void Sprite::Actions::submit(Reading, Id, system::Device::Id, renderer::CommandBuffer&) {
        // Needs sprite-pass material / atlas uniforms — next pipeline steps.
        _INCOMPLETE_;
    }

}
